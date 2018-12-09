
#include "webserver.h"
//	socket for client &	server
extern int client_sockfd;
extern int server_sockfd;

//	http header
extern char status[];
extern char header[];
extern char body[];

//	messgae send to client for LED control
extern char bt_on[];
extern char bt_off[];

//	user define function for ctrl+c
//	close socket before interrupt
//	avoid socket/bind ERR when run it next time
extern void signal_exit(int sig);

HANDLE hComm; 

int main(void)
{
	hComm = CreateFile(WEBSERVER_COM_PORT,GENERIC_READ | GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,0);
	printf("Error code for hcomm: %d\n",GetLastError());
	LEDStates ledstates;
	ledstates.RedState = FALSE;
	ledstates.GreenState = FALSE;
	ledstates.BlueState = FALSE;
	
	WSADATA wsaData;
	int iResult;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (iResult != 0) 
	{
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}
	server_sockfd = socket(AF_INET,SOCK_STREAM,0);
	if(-1 == server_sockfd)
	{
		perror("socket"),exit(-1);
	}
	printf("socket\n");

	struct sockaddr_in addr;
	memset(&addr,0,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port	= htons(80);
	addr.sin_addr.s_addr	= inet_addr("127.0.0.1");

	int res = bind(server_sockfd,(struct sockaddr*)&addr,sizeof(addr));
	printf("\r\n Error code: %d\n",WSAGetLastError());
	if(-1 == res)
	{
		perror("bind"),exit(-1);
	}
	
	res = listen(server_sockfd,100);
	if(-1 == res)
	{
		perror("listen"),exit(-1);
	}
	printf("listenning...\n");
	
	printf("close web server --- ctrl+c\n");
	
	if(SIG_ERR == signal(SIGINT,signal_exit))
	{
		perror("signal"),exit(-1);
	}
	// always ack client request
	while(1)
	{
		printf("test on\n");
		client_sockfd = accept(server_sockfd,NULL,NULL);
		if(-1 == client_sockfd)
		{
			perror("accept"),exit(-1);
		}
		int tmp_sockfd = client_sockfd;
		char buf[1024];
		recv(client_sockfd,buf,1024,0);
		printf("Buf length: %d", strlen(buf));
		printf("%s",buf);
		
		struct sockaddr_in recv_addr;
		socklen_t len = sizeof(recv_addr);
		getpeername(client_sockfd,(struct sockaddr*)&recv_addr,&len);
		char* ip = inet_ntoa(recv_addr.sin_addr);
		printf("client log in, ip is %s\n",ip);
		if(-1 == res)
		{
			perror("recv"),exit(-1);
		}
		char *str_bt0=NULL;
		str_bt0=strstr(buf,"button=0");
		char *str_bt1=NULL;
		str_bt1=strstr(buf,"button=1");
		char *str_bt2=NULL;
		str_bt2=strstr(buf,"button=2");
		
		char dataToSend;
		send(client_sockfd,status,sizeof(status),0);
		send(client_sockfd,header,sizeof(header),0);
		send(client_sockfd,body1,sizeof(body1),0);

		if(str_bt0!=NULL && str_bt1==NULL && str_bt2==NULL)
		{
			ledstates.RedState = !ledstates.RedState;
			dataToSend = 'r';
			WriteABuffer(&dataToSend,1);
		}	
		else if(str_bt0==NULL && str_bt1!=NULL && str_bt2==NULL)//if click LED OFF
		{
			ledstates.GreenState = !ledstates.GreenState;
			dataToSend = 'g';
			WriteABuffer(&dataToSend,1);
		}	
		else if(str_bt0==NULL && str_bt1==NULL && str_bt2!=NULL)
		{
			ledstates.BlueState = !ledstates.BlueState;
			dataToSend = 'b';
			WriteABuffer(&dataToSend,1);
		}
		else
		{
			//no button pressed, do nothing
		}
		if(ledstates.RedState)
		{
			send(client_sockfd,redOn,sizeof(redOn),0);
		}
		else
		{
			send(client_sockfd,redOff,sizeof(redOff),0);
		}
		if(ledstates.GreenState)
		{
			send(client_sockfd,greenOn,sizeof(greenOn),0);
		}
		else
		{
			send(client_sockfd,greenOff,sizeof(greenOff),0);
		}
		if(ledstates.BlueState)
		{
			send(client_sockfd,blueOn,sizeof(blueOn),0);
		}
		else
		{
			send(client_sockfd,blueOff,sizeof(blueOff),0);
		}
		send(client_sockfd,body2,sizeof(body2),0);
		close(client_sockfd);
	}
	WSACleanup();
	return 0;
}


BOOL WriteABuffer(char * lpBuf, DWORD dwToWrite)
{
   OVERLAPPED osWrite = {0};
   DWORD dwWritten;
   DWORD dwRes;
   BOOL fRes;
   printf("Entered function\n");

   // Create this write operation's OVERLAPPED structure's hEvent.
   osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
   if (osWrite.hEvent == NULL){
      // error creating overlapped event handle
	  printf("error 1");
      return FALSE;
   }

   // Issue write.
   if (!WriteFile(hComm, lpBuf, dwToWrite, &dwWritten, &osWrite)) {
      if (GetLastError() != ERROR_IO_PENDING) { 
         // WriteFile failed, but isn't delayed. Report error and abort.
		 printf("Error code: %d\n",GetLastError());
         fRes = FALSE;
      }
      else
	  {
         // Write is pending.
         dwRes = WaitForSingleObject(osWrite.hEvent, INFINITE);
         switch(dwRes)
         {
            // OVERLAPPED structure's event has been signaled. 
            case WAIT_OBJECT_0:
                 if (!GetOverlappedResult(hComm, &osWrite, &dwWritten, FALSE)){
					   printf("error 3");
                       fRes = FALSE;
				 }
                 else
                  // Write operation completed successfully.
                  fRes = TRUE;
                 break;
            
            default:
                 // An error has occurred in WaitForSingleObject.
                 // This usually indicates a problem with the
                // OVERLAPPED structure's event handle.
				 printf("error 2\n");
                 fRes = FALSE;
                 break;
         }
      }
   }
   else
      // WriteFile completed immediately.
      fRes = TRUE;

   CloseHandle(osWrite.hEvent);
   return fRes;
}




