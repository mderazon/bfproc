
#include "common.h"


/* struct to hold the node's neighbors */
struct neighbor {
	bool started;
	int procid;
	char *ip;
	unsigned short port;	
	int cost;
};

/*
 *	struct to hold the node's data. 
 *	the threads are going to change some of these fields constantly
 */
struct nodeData {
	bool updated;
	struct neighbor* neighbors;
	int procid;
	int parent;
	unsigned short localport;
	DWORD STARTUPTIME;
	DWORD SHUTDOWNTIME;
	DWORD LIFETIME;
	DWORD HELLOTIMEOUT;
	DWORD MAXTIME;
	int myRoot;
	int myCost;
	DWORD myRootTime;	
	HANDLE mutex;
};

/*	return true if LIFETIME of the node has expired and false o.w. */
bool lifetimeExpired(struct nodeData* nodeData) {
	return GetTickCount() > (nodeData->SHUTDOWNTIME);
}

/* deserialize the message received
char* ptr = recvBuf;
int rv;
serialize the data members to be sent 
rv = sprintf(ptr,"%d",nodeData->myRoot);
ptr += rv;
*ptr = ' ';
rv = sprintf(ptr,"%d",nodeData->myCost);
ptr += rv;
*ptr = ' ';
rv = sprintf(ptr,"%d",nodeData->procid);
ptr += rv;
*ptr = ' ';
rv = sprintf(ptr,"%d",nodeData->myRootTime);
ptr += rv;
*ptr = 0;
*/

/*	
*	this function gets a buffer that contains a message from one of the neighbors nodes
*	and updates this node's parameters (or not) according to the new information
*/
int updateNode(struct nodeData* nodeData, char* recvBuf,int neighbor) {
	char* ptr = recvBuf;
	int rv;
	DWORD waitResult;
	int newRoot, newCost, newId;
	DWORD newRootTime;
	/* deserialize the data members fom the buffer */
	rv = sscanf(ptr,"%d",newRoot);
	ptr += rv+1;
	rv = sscanf(ptr,"%d",newCost);
	ptr += rv+1;
	rv = sscanf(ptr,"%d",newId);
	ptr += rv+1;
	rv = sscanf(ptr,"%d",newRootTime);
	/* wait for the mutex to be free. block until it's available. */
	waitResult = WaitForSingleObject(nodeData->mutex,INFINITE);
	switch (waitResult)
	{
		/* the thread got ownership of the mutex */
		case WAIT_OBJECT_0: 
			__try { 
				int updatedMyCost = nodeData->myCost + nodeData->neighbors[neighbor].cost;
				int updatedNewCost = newCost + nodeData->neighbors[neighbor].cost ;
				/* check the conditions to update the node data according to the new message */
				if((newRoot < nodeData->myRoot)||
					(newRoot == nodeData->myRoot && updatedNewCost < updatedMyCost )||
					(newRoot == nodeData->myRoot && updatedNewCost == updatedMyCost &&
					nodeData->neighbors[neighbor].procid < nodeData->procid))
				{
					nodeData->updated = true;
					nodeData->myRoot = newRoot;
					nodeData->myCost = updatedNewCost;
					nodeData->parent = nodeData->neighbors[neighbor].procid;
					nodeData->myRootTime = newRootTime /* - TODO passed time since */;
				}
				/* nothing to update */
				else nodeData->updated = false;
			} 	
			__finally { 
				/* release ownership of the mutex object in any case */
				if (! ReleaseMutex(nodeData->mutex)) { 
					fprintf(stderr, "ReleaseMutex(): unable to release mutex");
					return -1;
				} 
			} 
			break; 
			// The thread got ownership of an abandoned mutex. something bad happened... */
		case WAIT_ABANDONED: 
			return -1; 
	}


}

/*
*	this thread is responsible of sending update messages to one of the neighbors 
*	each neighbor will get it's own thread
*/
int neighborThread(struct nodeData* nodeData){
	// TODO
}

/*
*	this thread is responsible for listening for update messages on localport
*	from other nodes. only one thread like this exist during the run of the program
*	and when LIFETIME expires the thread shuts down and the program ends
*/
int listenerThread(struct nodeData* nodeData) {
	char recvBuf[BUF_LEN];
	sockaddr_in SenderAddr;
	int SenderAddrSize = sizeof (SenderAddr);


	/* create new socket for listener */
	SOCKET listenSocket;
	if(!createSocket(listenSocket))
	{
		WSACleanup();
		exit(-1);
	}
	/* bind socket to localport */
	if(!bindSocket(listenSocket,nodeData->localport)){
		closesocket(listenSocket);
		WSACleanup();
		exit(-1);
	}
	int rv;
	/* continue listen for messages for entire lifetime of node */
	while (!lifetimeExpired(nodeData)) {
		/* perform a quick select, make sure we don't block too much */
		if (quickSelect(listenSocket, (nodeData->SHUTDOWNTIME - GetTickCount()))) {
			rv = recvfrom(listenSocket, recvBuf, BUF_LEN, 0, (SOCKADDR *) & SenderAddr, &SenderAddrSize);
			if (rv < 0) {
				fprintf(stderr,"recvfrom() failed: %ld.\n", WSAGetLastError());
				exit(-1);
			}
			updateNode(nodeData, recvBuf);
		}
	}
	exit(0);
}


void main(int argc,char* argv[]) {


	struct nodeData thisNode;

	/* INITIALIZATION AND VALIDATION STUFF */

	/* make sure the number of arguments are correct */
	if ((argc <= 9) || ((argc-6)%3)!=0) {
		fprintf(stderr, "usage: %s <procid> <localport> <lifetime> <hellotimeout> <maxtime> <ipaddress1> <port1> <cost1> ...\n",argv[0]);
		printf("Press any key to continue\n");
		int i; scanf("%d",&i);
		exit(-1);
	}

	char* procid_arg = argv[1];
	char* localport_arg = argv[2];
	char* lifetime_arg = argv[3];
	char* hellotimeout_arg = argv[4];
	char* maxtime_arg = argv[5];

	/* extract and validate local port argument */
	unsigned short localport;
	int rv = sscanf(localport_arg,"%u",&localport);
	if(rv < 0 || localport < MIN_PORT || localport > MAX_PORT){
		fprintf(stderr, "invalid local port num [%s]\n",localport_arg);
		exit(-1);
	}
	thisNode.localport = localport;
	/* extract and validate lifetime argument */
	DWORD lifetime;
	rv = sscanf(lifetime_arg,"%u",&lifetime);
	if(rv < 0){
		fprintf(stderr, "invalid LIFETIME value [%s]\n",lifetime_arg);
		exit(-1);
	}
	thisNode.LIFETIME = lifetime * 1000;
	/* extract and validate hellotimeout argument */
	DWORD hellotimeout;
	rv = sscanf(hellotimeout_arg,"%u",&hellotimeout);
	if(rv < 0){
		fprintf(stderr, "invalid HELLOTIMEOUT value [%s]\n",hellotimeout_arg);
		exit(-1);
	}
	thisNode.HELLOTIMEOUT = hellotimeout * 1000;
	/* extract and validate maxtime argument */
	DWORD maxtime;
	rv = sscanf(maxtime_arg,"%u",&maxtime);
	if(rv < 0){
		fprintf(stderr, "invalid MAXTIME value [%s]\n",maxtime_arg);
		exit(-1);
	}
	thisNode.MAXTIME = maxtime * 1000;

	/* process the neighbors list */
	int numOfNeighbors = (argc-6)/3;
	thisNode.neighbors = (neighbor*) malloc(numOfNeighbors * sizeof(struct neighbor));
	for (int i = 0; i < numOfNeighbors; i++){
		/* validates neighbor's IP */
		char *neighborIP = argv[i+6];		
		if(!validIP(neighborIP)){
			fprintf(stderr, "invalid neighbor's ip address [%s]\n",neighborIP);
			printf("Press any key to continue\n");
			int j; scanf("%d",&j);
			exit(-1);
		}
		thisNode.neighbors[i].ip = neighborIP;		
		/* validates neighbor's port */
		unsigned short neighborPort;
		int rv = sscanf(argv[i+7],"%u",&neighborPort);
		if(rv < 0 || neighborPort < MIN_PORT || neighborPort > MAX_PORT){
			fprintf(stderr, "invalid neighbor port num [%s]\n",argv[i+7]);
			printf("Press any key to continue\n");
			int j; scanf("%d",&j);
			exit(-1);
		}
		thisNode.neighbors[i].port = neighborPort;
		/* validates neighbor's port */
		int neighborCost;
		rv = sscanf(argv[i+8],"%u",&neighborCost);
		if(neighborCost < 1){
			fprintf(stderr, "invalid neighbor's cost [%s]\n",argv[i+8]);
			printf("Press any key to continue\n");
			int j; scanf("%d",&j);
			exit(-1);
		}
		thisNode.neighbors[i].cost = neighborCost;
		thisNode.neighbors[i].started = false;
	}
	/* turn the update flag on for the node to signal that it's okay to send updates */
	thisNode.updated = true;
	/* initialize the mutex */
	thisNode.mutex = CreateMutex(NULL, FALSE, NULL);
	if (thisNode.mutex == NULL){
		fprintf(stderr, "CreateMutex error: %d\n", GetLastError());
		exit(-1);
	}
	/* initialize the time the node is up and running */
	thisNode.STARTUPTIME = GetTickCount();
	thisNode.SHUTDOWNTIME = thisNode.STARTUPTIME + thisNode.LIFETIME;

	/* initialize WinSock Library */
	if(!initWSA()) exit(-1);

	/* END OF INITIALIZATION AND VALIDATION STUFF */

	/* TODO create listener thread */

	/* TODO create neighbors threads */

	free(thisNode.neighbors);
	exit(0);
}

