#!/usr/bin/python

import getpass
import sys
import telnetlib

words = ["helo",
	"hello",
	"helllllllllllllllllllllllllllllllllllllllllllllllllllllloooooooooooooooooooooooooooooooooooooooooooooo",
	"$%$#$#$#@$#@$@#$@#",
	"theladfjadsfoiajer",
	"end",
	"heliothon",
	"",
	"bird",
	"bid",
	"bil",
	"GET /ad.jpg",
	"GET /mod?healkfdjlkajfdas",
	"GET /mod?healdaflkhjdsalkfjalkfdsjlkdsakfdjlkajfdas",
	"GET mod/hello.htm?hello"
	"GET /mod/hello.htm?hello"
	"."]
HOST = "localhost"


for i in words: 
	print("Trying: "+i)
	try:
		tn = telnetlib.Telnet(HOST,8095)
		tn.write(i+"\n")
		print tn.read_until("hey",5)
	except:
		print("done!\n")
		
