
#include "common.h"

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