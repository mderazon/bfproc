
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <winsock2.h>
#include <time.h>
#include <windows.h>
#include <math.h>

#include <conio.h>
#include <sys/stat.h>



#define MIN_PORT 1024
#define MAX_PORT 65535

/*	The function checks if the given String (arg_IP)
*	represents a legal IP-Address.
*	It returns yes if it is or false if is isn't.
*/
bool validIP(char* arg_IP);