
#include <stdlib.h>
#include <stdio.h>
#include <string>
//#include <sstream>
#include <winsock2.h>
#include <time.h>
//#include <windows.h>




#define BUF_LEN 1024
#define MIN_PORT 1024
#define MAX_PORT 65535

bool quickSelect(SOCKET sock, DWORD time_out);

/*	The function Initializes the WSA global handler
*	It returns true for success of false for failure
*/
bool initWSA();


/*	The function closes notidies that we had a socket error.
*	It can get an extra text message using pMessage.	
*/
void notifySocketError(const char* pMessage = NULL);

/*	The function checks if the given String (arg_IP)
*	represents a legal IP-Address.
*	It returns yes if it is or false if is isn't.
*/
bool validIP(char* arg_IP);

/*	The function creates a UDP socket and pass using sock
*	It returns true if the creation succeeded or false for error
*/
bool createSocket(SOCKET& sock);

/*	The function binds a given socket to any address and the given port.
*	It returns true if the bounding succeeded or false for error
*/
bool bindSocket(SOCKET sock,unsigned short port);