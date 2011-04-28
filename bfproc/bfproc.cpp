#include "common.h"

/* struct to hold the node's neighbors  */
struct neighbor {
	DWORD lastMessageRecvTime;
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
	HANDLE updateEvent;				// a handle used to signal update to the process view
	struct neighbor* neighbors;		// neighbors array
	int numOfNeighbors;				// number of neighbors
	int procid;						// this process id
	int parentNum;					// the parents nmber in the parents array
	unsigned short localport;
	DWORD STARTUPTIME;				// the absolute time the program started
	DWORD SHUTDOWNTIME;				// the absolute shutdown time of the program
	DWORD LIFETIME;					// the amount of time the program runs
	DWORD lastMessagSentTime;
	DWORD HELLOTIMEOUT;
	DWORD MAXTIME;
	int myRoot;
	int myCost;
	DWORD myRootTime;
	HANDLE mutex;					// a mutex for thread safe work
	SOCKET sock;					// the socket associated with the process
};

/*	return true if LIFETIME of the node has expired and false o.w. */
bool lifetimeExpired(struct nodeData* nodeData) {
	return GetTickCount() > (nodeData->SHUTDOWNTIME);
}


/*	take the node data and put it as a string in buf so it could be sent to all neighbors 
*	sets the data as the following string: "myRoot myCost procid myRootTime" */
int serializeMessage(struct nodeData* nodeData, char* buf) {
	char* ptr = buf;
	int rv;
	DWORD waitResult;

	/* wait for the mutex to be free. block until it's available. */
	waitResult = WaitForSingleObject(nodeData->mutex,INFINITE);
	switch (waitResult)
	{
		/* the thread got ownership of the mutex. entering critical section */
	case WAIT_OBJECT_0: 
		__try { 
			rv = sprintf(ptr,"%d %d %d %d\n",
				nodeData->myRoot,
				nodeData->myCost, 
				nodeData->procid, 
				(nodeData->myRootTime==INFINITE?nodeData->MAXTIME:nodeData->myRootTime - 
				(GetTickCount() - nodeData->neighbors[nodeData->parentNum].lastMessageRecvTime)));
		}
		/* release ownership of the mutex object in any case */
		__finally { 			
			if (! ReleaseMutex(nodeData->mutex)) {
				fprintf(stdout, "ReleaseMutex(): unable to release mutex\n");
				return -1;
			}
		}
		break; 
		/* the thread got ownership of an abandoned mutex. something bad happened... */
	case WAIT_ABANDONED: 
		return -1; 

	}
	return 0;
}


int sendMessage(struct nodeData* nodeData){
	int rv, i;
	struct sockaddr_in recvAddr;
	char sendBuf[BUF_LEN];
	/* create new socket to send to neighbor */

	serializeMessage(nodeData, sendBuf);
	int bufLength = strlen(sendBuf);
	for (i=0; i<nodeData->numOfNeighbors; i++){
		recvAddr.sin_family = AF_INET;
		recvAddr.sin_port = htons(nodeData->neighbors[i].port);
		recvAddr.sin_addr.s_addr  = inet_addr(nodeData->neighbors[i].ip);
		rv = sendto(nodeData->sock, sendBuf, bufLength, 0, (SOCKADDR *) & recvAddr, sizeof (recvAddr));
		if (rv < 0) {
			fprintf(stdout,"sendto() failed to send to neighbor id %d with error: %d\n",nodeData->neighbors[i].procid, WSAGetLastError());
			closesocket(nodeData->sock);
			WSACleanup();
			return -1;
		}
	}
}

/*
*	this thread is responsible of sending update messages to all the neighbors.
*	a message is sent iff one of the following holds:
*	1) the process view (namely, the value of myRoot, myCost, myRootTime) has changed.
*	2) every HELLO_TIMEOUT period of time.
*/
DWORD WINAPI neighborsThread(LPVOID threadParam){
	struct nodeData* nodeData = (struct nodeData*) threadParam;
	DWORD waitResult;

	/* as long as the program is running, wait for an update event */
	while(1) {
		waitResult = WaitForSingleObject(nodeData->updateEvent, nodeData->HELLOTIMEOUT);
		DWORD elapsedTime = GetTickCount() - nodeData->STARTUPTIME;
		switch (waitResult) 
		{
			/* we got an update to the node - sending updates to neighbors */
		case WAIT_OBJECT_0: 
			sendMessage(nodeData);
			fprintf(stdout,"time=%d.%d\tSent update to neighbors\n",elapsedTime/1000,(elapsedTime % 1000)/10);
			ResetEvent(nodeData->updateEvent); /* reset the event back to unsignaled */
			break;
			/* we didn't get an update but it's time to send updates to neighbors anyway */
		case WAIT_TIMEOUT:
			sendMessage(nodeData);
			fprintf(stdout,"time=%d.%d\tSent HELLO-TIMEOUT update to neighbors\n",elapsedTime/1000,(elapsedTime % 1000)/10);
			break;
			/* something bad happened */
		default: 
			printf("WaitForSingleObject() error: (%d)\n", GetLastError()); 
			return 0; 
		}

	}
}


/* gets sockaddr_in of the sneder of the message and returns the neighbor's number in this node neighbors list */
int whichNeighbor(struct nodeData* nodeData, struct sockaddr_in* SenderAddr){
	unsigned short senderPort = ntohs(SenderAddr->sin_port);
	char senderIP[BUF_LEN];
	strcpy(senderIP, inet_ntoa(SenderAddr->sin_addr));	
	int i;
	/* look for the neighbor in the nodeData neighbors list */
	for(i=0; i < nodeData->numOfNeighbors; i++){
		if (!strcmp(senderIP,nodeData->neighbors[i].ip) && senderPort == nodeData->neighbors[i].port){
			return i;
		}
	}
	/* neighbor not found returning error */
	return -1;
}

/*	
*	this function gets a buffer that contains a message from one of the neighbors nodes
*	and updates this node's parameters (or not) according to the new information
*/
int updateNode(struct nodeData* nodeData, char* recvBuf,int neighbor) {
	char* ptr = recvBuf;
	bool updated = false;
	int rv;
	DWORD waitResult;
	int newRoot, newCost, neighborId;
	DWORD newRootTime;
	/* deserialize the data members from the buffer */
	rv = sscanf(ptr,"%d %d %d %d",&newRoot, &newCost, &neighborId, &newRootTime);

	int updatedNewCost = newCost + nodeData->neighbors[neighbor].cost ;
	/* check the conditions to update the node data according to the new message */
	if((newRoot < nodeData->myRoot)||
		(newRoot == nodeData->myRoot && updatedNewCost < nodeData->myCost )||
		(newRoot == nodeData->myRoot && updatedNewCost == nodeData->myCost &&
		nodeData->neighbors[neighbor].procid < nodeData->neighbors[nodeData->parentNum].procid))
	{
		/* wait for the mutex to be free. block until it's available. */
		waitResult = WaitForSingleObject(nodeData->mutex,INFINITE);
		switch (waitResult)
		{
			/* the thread got ownership of the mutex. entering critical section */
		case WAIT_OBJECT_0: 
			__try { 
				/* check if this is the first time we get this neighbor so we can update it's process id */
				if (nodeData->neighbors[neighbor].procid == -1)		
					nodeData->neighbors[neighbor].procid = neighborId;
				nodeData->myRoot = newRoot;
				nodeData->myCost = updatedNewCost;
				nodeData->parentNum = neighbor;
				nodeData->myRootTime = newRootTime;
				SetEvent(nodeData->updateEvent);
				updated = true;
			} 	
			__finally { 
				/* release ownership of the mutex object in any case */
				if (! ReleaseMutex(nodeData->mutex)) {
					fprintf(stdout, "ReleaseMutex(): unable to release mutex\n");
					return -1;
				}
			}
			break; 
			/* the thread got ownership of an abandoned mutex. something bad happened... */
		case WAIT_ABANDONED: 
			return -1; 
		}
	}
	/* check for a crash */
	DWORD elapsedTime = GetTickCount() - nodeData->STARTUPTIME;
	if ((GetTickCount() > nodeData->neighbors[nodeData->parentNum].lastMessageRecvTime + nodeData->MAXTIME &&
		nodeData->neighbors[nodeData->parentNum].lastMessageRecvTime != 0)||
		neighbor == nodeData->parentNum && newRootTime > nodeData->myRootTime && nodeData->neighbors[neighbor].procid != -1){
			waitResult = WaitForSingleObject(nodeData->mutex,INFINITE);
			if (waitResult == WAIT_OBJECT_0){
				nodeData->neighbors[nodeData->parentNum].lastMessageRecvTime = 0;
				nodeData->myCost = 0;
				nodeData->myRoot = nodeData->procid;
				nodeData->myRootTime = INFINITE;
				nodeData->parentNum = -1;
				updated = true;
				fprintf(stdout, "time=%d.%d\tCrash detected.\n",elapsedTime/1000,(elapsedTime % 1000)/10,nodeData->neighbors[neighbor].ip);
			}
			ReleaseMutex(nodeData->mutex);
	}
	/* check if we updated */
	elapsedTime = GetTickCount() - nodeData->STARTUPTIME;
	if (updated) {
		fprintf(stdout, "time=%d.%d\tReceived update from %s\n",elapsedTime/1000,(elapsedTime % 1000)/10,nodeData->neighbors[neighbor].ip);
		fprintf(stdout, "time=%d.%d\troot:%d parent:%s distance:%d\n",elapsedTime/1000,(elapsedTime % 1000)/10,
			nodeData->myRoot,nodeData->neighbors[neighbor].ip,nodeData->myCost);
	}
	/* nothing to update */
	else {
		fprintf(stdout, "time=%d.%d\tReceived update from %s, No change.\n",elapsedTime/1000,(elapsedTime % 1000)/10,nodeData->neighbors[neighbor].ip);
	}
	nodeData->neighbors[neighbor].lastMessageRecvTime = GetTickCount();
	return 0;
}

/*
*	this thread is responsible for listening for update messages on localport
*	from other nodes. only one thread like this exist during the run of the program
*	and when LIFETIME expires the thread shuts down and the program ends
*/
DWORD WINAPI listenerThread(LPVOID threadParam) {
	struct nodeData* nodeData = (struct nodeData*) threadParam;
	char recvBuf[BUF_LEN];
	sockaddr_in SenderAddr;
	int SenderAddrSize = sizeof (SenderAddr);

	DWORD elapsedTime = GetTickCount() - nodeData->STARTUPTIME;
	fprintf(stdout, "time=%d.%d\troot:%d parent:NULL distance:0\n",elapsedTime/1000,(elapsedTime % 1000)/10,
		nodeData->myRoot);

	int rv;
	/* continue listen for messages for entire lifetime of node */
	while (!lifetimeExpired(nodeData)) {
		/* perform a quick select, make sure we don't block too much */
		if (quickSelect(nodeData->sock, (nodeData->SHUTDOWNTIME - GetTickCount()))) {
			rv = recvfrom(nodeData->sock, recvBuf, BUF_LEN, 0, (SOCKADDR *) & SenderAddr, &SenderAddrSize);
			if (rv < 0) {
				fprintf(stdout,"recvfrom() failed: %ld.\n", WSAGetLastError());
				exit(-1);
			}
			/* check from which neighbor this message came from */
			int neighborId = whichNeighbor(nodeData, &SenderAddr);
			if (neighborId == -1){
				fprintf(stdout,"message received from an unfamiliar source\n");
			}
			/* update the node if necessary */
			updateNode(nodeData, recvBuf, neighborId);
		}
	}
	exit(0);
}


void main(int argc,char* argv[]) {


	struct nodeData thisNode;

	/* INITIALIZATION AND VALIDATION STUFF */

	/* make sure the number of arguments are correct */
	if ((argc < 9) || ((argc-6)%3)!=0) {
		fprintf(stdout, "usage: %s <procid> <localport> <lifetime> <hellotimeout> <maxtime> <ipaddress1> <port1> <cost1> ...\n",argv[0]);
		printf("Press any key to continue\n");
		int i; scanf("%d",&i);
		exit(-1);
	}

	char* procid_arg = argv[1];
	char* localport_arg = argv[2];
	char* lifetime_arg = argv[3];
	char* hellotimeout_arg = argv[4];
	char* maxtime_arg = argv[5];

	/* extract and validate the process id argument */
	int procid;
	int rv = sscanf(procid_arg,"%d",&procid);
	if(rv < 0){
		fprintf(stdout, "invalid process id [%s]\n",procid_arg);
		exit(-1);
	}
	thisNode.procid = procid;
	/* extract and validate local port argument */
	unsigned short localport;
	rv = sscanf(localport_arg,"%u",&localport);
	if(rv < 0 || localport < MIN_PORT || localport > MAX_PORT){
		fprintf(stdout, "invalid local port num [%s]\n",localport_arg);
		exit(-1);
	}
	thisNode.localport = localport;
	/* extract and validate lifetime argument */
	DWORD lifetime;
	rv = sscanf(lifetime_arg,"%u",&lifetime);
	if(rv < 0){
		fprintf(stdout, "invalid LIFETIME value [%s]\n",lifetime_arg);
		exit(-1);
	}
	thisNode.LIFETIME = lifetime * 1000;
	/* extract and validate hellotimeout argument */
	DWORD hellotimeout;
	rv = sscanf(hellotimeout_arg,"%u",&hellotimeout);
	if(rv < 0){
		fprintf(stdout, "invalid HELLOTIMEOUT value [%s]\n",hellotimeout_arg);
		exit(-1);
	}
	thisNode.HELLOTIMEOUT = hellotimeout * 1000;
	/* extract and validate maxtime argument */
	DWORD maxtime;
	rv = sscanf(maxtime_arg,"%u",&maxtime);
	if(rv < 0){
		fprintf(stdout, "invalid MAXTIME value [%s]\n",maxtime_arg);
		exit(-1);
	}
	thisNode.MAXTIME = maxtime * 1000;
	thisNode.myCost = 0;
	thisNode.myRoot = thisNode.procid;
	thisNode.myRootTime = INFINITE;

	/* process the neighbors list */
	int numOfNeighbors = (argc-6)/3;
	thisNode.numOfNeighbors = numOfNeighbors;
	thisNode.neighbors = (neighbor*) malloc(numOfNeighbors * sizeof(struct neighbor));
	for (int i = 0; i < numOfNeighbors; i++){
		int k = 3*i;
		/* validates neighbor's IP */
		char *neighborIP = argv[k+6];		
		if(!validIP(neighborIP)){
			fprintf(stdout, "invalid neighbor's ip address [%s]\n",neighborIP);
			printf("Press any key to continue\n");
			int j; scanf("%d",&j);
			exit(-1);
		}
		thisNode.neighbors[i].ip = neighborIP;		
		/* validates neighbor's port */
		unsigned short neighborPort;
		int rv = sscanf(argv[k+7],"%u",&neighborPort);
		if(rv < 0 || neighborPort < MIN_PORT || neighborPort > MAX_PORT){
			fprintf(stdout, "invalid neighbor port num [%s]\n",argv[k+7]);
			printf("Press any key to continue\n");
			int j; scanf("%d",&j);
			exit(-1);
		}
		thisNode.neighbors[i].port = neighborPort;
		/* validates neighbor's cost */
		int neighborCost;
		rv = sscanf(argv[k+8],"%u",&neighborCost);
		if(neighborCost < 1){
			fprintf(stdout, "invalid neighbor's cost [%s]\n",argv[k+8]);
			printf("Press any key to continue\n");
			int j; scanf("%d",&j);
			exit(-1);
		}
		thisNode.neighbors[i].cost = neighborCost;
		/* reset neighbor process id to be -1 */
		thisNode.neighbors[i].procid = -1;
	}
	/* create updateEvent flag with initial state true  */
	thisNode.updateEvent = CreateEvent(NULL, TRUE, TRUE, TEXT("updatedNode"));
	/* initialize the mutex */
	thisNode.mutex = CreateMutex(NULL, FALSE, NULL);
	if (thisNode.mutex == NULL){
		fprintf(stdout, "CreateMutex error: %d\n", GetLastError());
		exit(-1);
	}
	/* initialize this node's parent to be 0 */
	thisNode.parentNum = -1;
	/* initialize the time the node is up and running */
	thisNode.STARTUPTIME = GetTickCount();
	thisNode.SHUTDOWNTIME = thisNode.STARTUPTIME + thisNode.LIFETIME;
	thisNode.lastMessagSentTime = 0;



	/* initialize WinSock Library */
	if(!initWSA()) exit(-1);
	/* create socket for communication */
	if(!createSocket(thisNode.sock))
	{
		WSACleanup();
		exit(-1);
	}
	/* bind the socket to use the localport for sending and receiving */ 
	if(!bindSocket(thisNode.sock, thisNode.localport)){
		closesocket(thisNode.sock);
		WSACleanup();
		exit(-1);
	}	

	/* END OF INITIALIZATION AND VALIDATION STUFF */

	/* create listener thread */
	HANDLE ListenerThreadHandle = CreateThread( 
		NULL,					// default security attributes
		0,						// use default stack size  
		listenerThread,			// thread function name
		&thisNode,		        // argument to thread function 
		0,						// use default creation flags 
		NULL);					// returns the thread identifier 
	if (ListenerThreadHandle == NULL) 
	{
		fprintf(stdout, "error creating listener thread\n");
		ExitProcess(3);
	}

	/* create neighbors threads */
	HANDLE NeighborsThreadHandle = CreateThread( 
		NULL,					// default security attributes
		0,						// use default stack size  
		neighborsThread,		// thread function name
		&thisNode,		        // argument to thread function 
		0,						// use default creation flags 
		NULL);					// returns the thread identifier 
	if (NeighborsThreadHandle == NULL) 
	{
		fprintf(stdout, "error creating neighbors thread\n");
		ExitProcess(3);
	}
	/* wait for the thread to finish */
	WaitForSingleObject(ListenerThreadHandle, INFINITE);
	free(thisNode.neighbors);
	closesocket(thisNode.sock);
	WSACleanup();
	exit(0);
}

