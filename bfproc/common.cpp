
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

/*	The function creates a UDP socket and pass using sock
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

	/* fix for error WSAECONNRESET on recvfrom() as published on website http://www.eng.tau.ac.il/networks/assignments/pa2_faq.htm */
	DWORD dwBytesReturned = 0;
	bool bNewBehavior = false;
	DWORD status = ::WSAIoctl(sock, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned,NULL, NULL);    
	if (status == SOCKET_ERROR)
	{    
		fprintf(stderr,"bind() failed: %ld.\n", WSAGetLastError());
	}       
	/* end of fix */

	return true;
}

bool quickSelect(SOCKET sock, DWORD time_out)
{
	// Create a read fd set
	fd_set read_fds;  

	// Keep track of the biggest file descriptor
	int fdmax = (int)sock; 

	// Keep trying until successful or error occurred

	// Use select with 100k usec timeout
	timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = time_out*1000;

	// clear the master and temp sets
	FD_ZERO(&read_fds);

	// Save the server socket on read_fds set
	FD_SET(sock, &read_fds);
	// Perform select with one second timeout
	int status = select(fdmax + 1, &read_fds, NULL, NULL, &timeout);
	if (status > 0)
	{
		// Is that the fd we were looking for?
		if (FD_ISSET(sock, &read_fds))
		{
			// Our socket is ready for reading!
			return true;
		}
	}
	else if (status == SOCKET_ERROR)
	{
		// Notify error and break the loop
		fprintf(stderr,"Unable to perform select\n");
		return false;
	}



	return false;
}