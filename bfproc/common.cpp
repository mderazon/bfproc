
#include "common.h"



/*	The function closes notidies that we had a socket error.
*	It can get an extra text message using pMessage.	
*/
void notifySocketError(const char* pMessage)
{
	// Get last socket error
	int sockError = WSAGetLastError();

	// Notify user we had a socket error
	fprintf(stderr, "Socket error [%d]", sockError);

	// prints the extra message
	if (pMessage)
	{
		fprintf(stderr, ": %s\n", pMessage);
	}
	else
	{		
		fprintf(stderr, "\n");
	}
}


/*	The function Initializes the WSA global handler
*	It returns true for success of false for failure
*/
bool initWSA()
{

	WSADATA wsaData;

	// Using MAKEWORD macro, Winsock version request 2.2
	WORD wVersionRequested = MAKEWORD(2, 2);

	// Initialize windows networking
	int wsaerr = WSAStartup(wVersionRequested, &wsaData);
	if (wsaerr == 0)
	{
		// Operation was successful
		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2 ){
			/* Tell the user that we could not find a usable WinSock DLL.*/
			fprintf(stderr,"The dll do not support the Winsock version %u.%u!\n", LOBYTE(wsaData.wVersion), HIBYTE(wsaData.wVersion));
			WSACleanup();
			return false;
		}
		return true;
	}
	else
	{
		// Notify we had a socket error
		notifySocketError("The Winsock dll wasn't found!\n");
		return false;
	}
}



/*	The function checks if the given String (arg_IP)
*	represents a legal IP-Address.
*	It returns yes if it is or false if is isn't.
*/
bool validIP(char* arg_IP){
	unsigned long ip_addr = inet_addr(arg_IP);
	if (ip_addr == INADDR_NONE){
		return false;
	}
	return true;
}

/*	The function creaes a UDP socket and pass using sock
*	It returns true if the creation succeeded or false for error
*/
bool createSocket(SOCKET& sock){
	SOCKET tmp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); 
    if (tmp_socket == INVALID_SOCKET)
    {
        fprintf(stderr,"socket() - Error at socket(): %ld\n", WSAGetLastError());
        return false;
    }
	sock = tmp_socket;
	return true;
}

/*	The function binds a given socket to any address and the given port.
*	It returns true if the bounding succeeded or false for error
*/
bool bindSocket(SOCKET sock,unsigned short port)
{
	struct sockaddr_in service;
	memset(&service, 0, sizeof(service));     
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = INADDR_ANY;
	service.sin_port = htons(port); 

	if (bind(sock, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR)
	{
		fprintf(stderr,"bind() failed: %ld.\n", WSAGetLastError());
		return false;
	}
	return true;
}