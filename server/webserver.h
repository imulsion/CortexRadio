#ifndef __WEBSERVER_H_
#define __WEBSERVER_H_
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <ws2tcpip.h>
#include <windows.h>

#define WEBSERVER_COM_PORT "\\\\.\\COM5"

//	socket for client &	server
int client_sockfd;
int server_sockfd;

//	http header
char status[] = "HTTP/1.1 200 OK";
char header[] = "Server: SimpleServer\r\nContent-Type: text/html;charset=utf-8\r\n\r\n";
char body1[] = \
			"<html>\
					<head><title>A simple Web Server</title></head>\
					<body>\
						<h1>COMP3215</h1>\
						<form action=\"\" method=\"post\">\
							<button name=\"button\" value='0' disable=\"disable\">RED TOGGLE</button>\
							<button name=\"button\" value='1' disable=\"disable\">GREEN TOGGLE</button>\
							<button name=\"button\" value='2' disable=\"disable\">BLUE TOGGLE</button>\
						</form>";
char body2[] = "</body></html>";


char redOn[]	=	"<h3>Red on</h3>";
char redOff[]	=	"<h3>Red off</h3>";
char greenOn[]  =   "<h3>Green on</h3>";
char greenOff[] =   "<h3>Green off</h3>";
char blueOn[]   =   "<h3>Blue on</h3>";
char blueOff[]  =   "<h3>Blue off</h3>";


typedef struct
{
	BOOL RedState;
	BOOL GreenState;
	BOOL BlueState;
}LEDStates;

BOOL WriteABuffer(char*,DWORD);

void signal_exit(int sig)
{
	printf("close web server, please wait....\n");
	sleep(3);
	int res = close(server_sockfd);
	if(-1 == res)
	{
		perror("close"),exit(-1);
	}
	printf("web server closed\n");
	exit(0);
}


#endif
