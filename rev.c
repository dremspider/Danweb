#include <string.h>

void reverse(char *str)
{
   int len;
   char ch;
   int i;

   if ( str == NULL ) {
      return;
   }

   len = strlen(str);

   for ( i = 0; i < len/2; i++ ) {
      ch = str[len - 1 - i];
      str[len - 1 - i] = str[i];
      str[i] = ch;
   }
}

char *def_mod(const char *arg) {
   char *str = strdup(arg);
   reverse(str);
   return str;
}

