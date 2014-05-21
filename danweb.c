//Used this to start... then had to change a lot of stuff... http://www.ibm.com/developerworks/systems/library/es-nweb/index.html
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <getopt.h>
#include <dlfcn.h>
#include <netdb.h>

#define BUFSIZE 8096
#define ERROR 42
#define SORRY 43
#define LOG   44
#define MAXTH 1000



//Declarations!
static void sig_handler(int signo);
void *get_in_addr(struct sockaddr *sa);
void chomp(const char *s);
void readConfig(char *path);
int daemonize();
void log(int type, char *s1, char *s2, int num);
void web(void *threadArgs);
int help();


//This will be used for locking the log file
pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER;

int port = 8080; //Port to listen on
//char *rootdir = "/tmp"; //Directory to serve out
//char *moddir = "mods";
//char *logfile = "/tmp/danweb";
char* rootdir;// = (char *)malloc(100*sizeof(char));
char* logfile;// = (char *)malloc(100*sizeof(char));
char* moddir ;//=  (char *)malloc(100*sizeof(char));

FILE *f;

int listenfd; //listener, need to be able to close it on sigint

//Used for what types of files are allowed
struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },  
	{"jpg", "image/jpeg"}, 
	{"jpeg","image/jpeg"},
	{"png", "image/png" },  
	{"zip", "image/zip" },  
	{"gz",  "image/gz"  },  
	{"tar", "image/tar" },  
	{"htm", "text/html" },  
	{"html","text/html" },  
	{0,0} };
//Used for short options
static const char *optString = "r:m:p:l:hc: ";

//used for long options
static const struct option longOpts[] = {
        {"rootdir", required_argument, NULL, 'r'},
        {"moddir", required_argument,NULL, 'm'},
        {"port", required_argument,NULL, 'p'},
        {"logfile", required_argument,NULL, 'l'},
        {"config", required_argument,NULL, 'c'},
        {"help", required_argument,NULL, 'h'}
};

//Catch sigints to close the listening handler
static void sig_handler(int signo)
{
    if ( signo == SIGINT ) {
		printf("Thank you for using danweb!\n");
		close(listenfd); //Close the listener to not create issues
		(void)funlockfile(f);
		exit(0);
	}
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}



//Gets rid of any new line character, equivelant to Perl's chomp
void chomp(const char *s) {
        char *p;
        while (NULL != s && (NULL != (p = strrchr(s, '\n')) || NULL != (p = strrchr(s, '\r'))))
                *p = '\0';
}


//read the configuration file that is specified and set any variable that is needed
void readConfig(char *path){
	FILE *fp;
        fp = fopen(path, "r");
        if(fp == NULL) {
                printf("Can't open file");
                exit(0);
        }
	//Read the file
	char line[100];
	//read each line
	while(fgets(line, 100, fp) != NULL) {
		chomp(line);
		char *split;
		split = strtok (line,"=");
		//Read the value 
		while (split != NULL) {
			if(!strncmp(split,"port",100)) {
				split = strtok(NULL, "=");
				port = atoi(split);
				printf("Setting port to %s\n",split);
			}
			if(!strncmp(split,"logfile",100)) {
				split = strtok(NULL, "=");
				strncpy(logfile,split,100);
				printf("Setting log file to %s\n",logfile);
			}
			if(!strncmp(split,"moddir",100)) {
				split = strtok(NULL, "=");
				strncpy(moddir,split,100);
				printf("Setting mod directory to %s\n",moddir);
			}
			if(!strncmp(split,"rootdir",100)) {
				split = strtok(NULL, "=");
				strncpy(rootdir,split,100);
				printf("Setting root directory to %s\n",rootdir);
			}
			split = strtok (NULL,"=");
		}
	}
}


//This holds the arguments to be passed in the web function
typedef struct {
	int fd;
	int hit;
	char *ip;
} threadStruct;


int daemonize() {
	pid_t pid;

	if ( (pid = fork()) < 0 ) {   // fork error
		return -1;
	} else if ( pid != 0 ) {      // parent should exit
		exit(0);
	}

	// child continues from here on

	// become a session leader
	setsid();

	// change working directory to / 
	chdir("/");

	// clear out file creation masks
	umask(0);

	return 0;
}

void log(int type, char *s1, char *s2, int num) {
	int fd ;
	char logbuffer[BUFSIZE*2];
	pthread_mutex_lock( &logMutex );

	switch (type) {
	case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",s1, s2, errno,getpid()); break;
	case SORRY: 
		(void)sprintf(logbuffer, "<HTML><BODY><H1>danweb Web Server Sorry: %s %s</H1></BODY></HTML>\r\n", s1, s2);
		(void)write(num,logbuffer,strlen(logbuffer));
		(void)sprintf(logbuffer,"SORRY: %s:%s",s1, s2); 
		break;
	case LOG: (void)sprintf(logbuffer,"%s:%s:%d",s1, s2,num); break;
	}	
	/* no checks here, nothing can be done a failure anyway */
	if((fd = open(logfile, O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		(void)write(fd,logbuffer,strlen(logbuffer)); 
		(void)write(fd,"\n",1);      
		(void)close(fd);
	}
	pthread_mutex_unlock( &logMutex );
}

/* this is a child web server process, so we can exit on errors */
void web(void *threadArgs) {
	int j, file_fd, buflen, len;
	long i, ret;
	char * fstr;
	char buffer[BUFSIZE+1]; /* static so zero filled */
	threadStruct *arg;
	arg = (threadStruct *) threadArgs;
	char *rest;
	char *modargs;
	chdir(rootdir);

	int fd = arg->fd;
	int hit = arg->hit;

	ret =read(fd,buffer,BUFSIZE); 	/* read Web request in one go */
	if(ret == 0 || ret == -1) {	/* read failure stop now */
		log(SORRY,"failed to read browser request","",fd);
	}
	if(ret > 0 && ret < BUFSIZE)	/* return code is valid chars */
		buffer[ret]=0;		/* terminate the buffer */
	else buffer[0]=0;

	for(i=0;i<ret;i++)	/* remove CF and LF characters */
		if(buffer[i] == '\r' || buffer[i] == '\n')
			buffer[i]='*';
	//log(LOG,"request",buffer,hit);
	
	char origRequest[1000];
	strncpy(origRequest,buffer,1000);

	if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) )
		log(SORRY,"Only simple GET operation supported",buffer,fd);

	for(i=4;i<BUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */
		if(buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
			buffer[i] = 0;
			break;
		}
	}

	for(j=0;j<i-1;j++) 	/* check for illegal parent directory use .. */
		if(buffer[j] == '.' && buffer[j+1] == '.')
			log(SORRY,"Parent directory (..) path names not supported",buffer,fd);


	//we need to see if mod is in the path.... If it is, we will need to handle it in a load the module
	for(j=0;j<i-1;j++) {
		//We found a mod
		if(buffer[j] == 'm' && buffer[j+1] == 'o'  && buffer[j+2] == 'd'  && buffer[j+3] == '/') {
			//rest is going to be what is after /mod
			rest = &buffer[4 + j];

			//OK, now we need to get the name of the mod.	
			char mod[100];
			int pointMove;
			for(int z=0;z<strlen(rest);z++) {
				if(rest[z] == '?') break;
				mod[z] = rest[z];
				pointMove++;
			}

			//Get all the arguments here
			modargs = &rest[pointMove];

			// Build out the mod path
			char path[200] = "";	
			strncat(path,moddir,200);
			strncat(path,mod,200);
			//double (*cosine)(double);
			char *(*modreturn)(char*);
			//char* modreturn;
			void* handle;
			char* error;

			handle = dlopen(path, RTLD_LAZY);
			if (!handle) {
				fputs (dlerror(), stderr);
			}
			modreturn = dlsym(handle, "def_mod");
			if ((error = dlerror()) != NULL)  {
       				fputs(error, stderr);
				return;
    			}
			//Pass in the returned pointer so that you could print out the data
			char *return2 = (*modreturn)(modargs);
			//(void)write(fd,return2,strlen(return2));
			(void)write(fd,return2,strlen(return2));
			dlclose(handle);
			goto cleanup;
		}
	}
		

	if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) /* convert no filename to index file */
		(void)strcpy(buffer,"GET /index.html");

	/* work out the file type and check we support it */
	buflen=strlen(buffer);
	fstr = (char *)0;
	for(i=0;extensions[i].ext != 0;i++) {
		len = strlen(extensions[i].ext);
		if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
			fstr =extensions[i].filetype;
			break;
		}
	}
	if(fstr == 0) log(SORRY,"file extension type not supported",buffer,fd);

	if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) /* open the file for reading */
		log(SORRY, "failed to open file",&buffer[5],fd);
//printf("opening the file: %s\n",&buffer[5]);

	
	//Need to get the date to send the date for the web server
	struct tm *local;
	time_t t;

	//Lets get the the current time
	t = time(NULL);
	local = localtime(&t);

	//Lets get the last modified date and size of the file
	struct tm      *tm;
	intmax_t size;
	struct stat statbuf;
	if (!stat(&buffer[5], &statbuf)) {
		tm = localtime(&statbuf.st_mtime);
		size = (intmax_t)statbuf.st_size;
	} 

	(void)sprintf(buffer,"HTTP/1.0 200 OK\r\nDate: %sLast-Modified: %sContent-Length: %9jd\n\rContent-Type: %s\r\n\r\n", asctime(local),asctime(tm),size,fstr);
	(void)write(fd,buffer,strlen(buffer));

	/* send file in 8KB block - last block may be smaller */
	while (	(ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
		(void)write(fd,buffer,ret);
	}


	//Lets finally log all the information we just created
	//client_ip [time] method uri status bytes-sent
	char logOutput[1000] = "";
	char *time = asctime(local);
	chomp(time);
	snprintf(logOutput, 1000, "%s: %s %s size:%9jd 202",arg->ip,time,origRequest,size);
	//snprintf(logOutput, 1000, "%s",arg->ip);
	log(LOG,"request",logOutput,hit);
	cleanup:
	close(fd);	
}

int help() {
	printf("hint: Welcome to danweb \n\n"
        "\tdanweb only servers out file/web pages with extensions named below\n"
        "\t and only from the named directory or its sub-directories.\n"
        "\tThere is no fancy features = safe and secure.\n\n"
        "\tOnly Supports:");
                for(int i=0;extensions[i].ext != 0;i++)
                        (void)printf(" %s",extensions[i].ext);

                exit(0);
}


int main(int argc, char *argv[]) {
	int rc;

	pthread_t threads[MAXTH];
	int threadCount = 0;

	int socketfd, hit;
	size_t length;
	static struct sockaddr_in cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */
	//Default directory
	int opt = 0;
	
	//Lets give the pointers some space!
	rootdir = (char *)malloc(100*sizeof(char));
	getcwd(rootdir,100);
	logfile = (char *)malloc(100*sizeof(char));
	getcwd(logfile,100);
	strncat(logfile,"/output",100);
	moddir = (char *)malloc(100*sizeof(char));
	getcwd(moddir,100);
	strncat(moddir,"/mod/",100);
	
	//Set up the signal handler to catch sigint
	signal(SIGINT,sig_handler);


        //grab options from getopts
	int longIndex = 0;
        opt = getopt_long(argc, argv, optString, longOpts, &longIndex);
	
        while(opt != -1) {

                switch (opt) {
                        case 'h':
                                help();
                                return 0;
                        case 'p':
				port = atoi(optarg);
                                printf("setting port to %d\n",port);
                                goto grabopt;
                        case 'r':
                                rootdir = optarg;
                               	printf("Setting rootdir to %s\n",rootdir);
                               	goto grabopt;
                        case 'l':
				//set the log file	
				logfile = optarg;
                               	printf("Setting logfile to %s\n", logfile);
                                goto grabopt;
			case 'c':
				//set the config
				readConfig(optarg);
				goto grabopt;
			case 'm':
				//set the moddir 
				moddir = optarg;
                               	printf("Setting moddir to %s\n", moddir);
				goto grabopt;
                        case '?':
                                if (optopt == 'p' || optopt == 'd' || optopt == 'l' || 
							optopt == 'c' || optopt == 'm')
                                	fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                                else if (isprint (optopt)) {
                                        fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                                        exit(0);
                                }
                }
                grabopt:
                //grab the next option.
		
                opt = getopt_long(argc, argv, optString, longOpts, &longIndex);
        }

	//Lets lock the log file
	//open(logfile, O_CREAT| O_WRONLY | O_APPEND,0644);
	//f=fopen(logfile,"w+");
	//flockfile(f, LOCK_EX);

	//Become a daemon
	daemonize();

	/* setup the network socket */
	//port = atoi(argv[1]);
	printf("port will be set to %d\n",port);
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
		log(ERROR, "system call","socket",0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
		log(ERROR,"system call","bind",0);
	if( listen(listenfd,64) <0)
		log(ERROR,"system call","listen",0);
	for(hit=1; ;hit++) {
//printf("in loop for hit\n");
		length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			log(ERROR,"system call","accept",0);

		//Lets get the Ip address information so that we record information
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, get_in_addr((struct sockaddr *)&cli_addr), ip, INET_ADDRSTRLEN);

//printf("socketfd: %d",socketfd);
//printf("after accept\n");

		if(threadCount < MAXTH) {
//printf("hey");
			threadStruct args;
			args.fd = socketfd;
			args.hit = hit;
			args.ip = ip;
//printf("calling web\n");
			rc = pthread_create(&threads[threadCount], NULL, web, (void *)&args);
			threadCount++;
		}
	}
}
