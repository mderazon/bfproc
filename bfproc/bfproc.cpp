
#include "common.h"

/* struct to hold the process neighbors */
typedef struct neighbor {
	char *ip;
	unsigned short port;	
	int cost;
};



void main(int argc,char* argv[]) {

	/* make sure the number of arguments are correct */
	if ((argc <= 9) || ((argc-6)%3)!=0) {
		fprintf(stderr, "usage: %s <procid> <localport> <lifetime> <hellotimeout> <maxtime> <ipaddress1> <port1> <cost1> ...\n",argv[0]);
		printf("Press any key to continue\n");
		int i; scanf("%d",&i);
		exit(-1);
	}

	char* procid = argv[1];
	char* localport = argv[2];
	char* lifetime = argv[3];
	char* hellotimeout = argv[4];
	char* maxtime = argv[5];

	/* process the neighbors list */
	int numOfNeighbors = (argc-6)/3;
	neighbor* neighbors = (neighbor*) malloc(numOfNeighbors * sizeof(neighbor));
	for (int i = 0; i < numOfNeighbors; i++){
		/* validates neighbor's IP */
		char *neighborIP = argv[i+6];		
		if(!validIP(neighborIP)){
			fprintf(stderr, "invalid neighbor's ip address [%s]\n",neighborIP);
			printf("Press any key to continue\n");
			int j; scanf("%d",&j);
			exit(-1);
		}
		neighbors[i].ip = neighborIP;
		
		/* validates neighbor's port */
		unsigned short neighborPort;
		int rv = sscanf(argv[i+7],"%u",&neighborPort);
		if(rv < 0 || neighborPort < MIN_PORT || neighborPort > MAX_PORT){
			fprintf(stderr, "invalid neighbor's port num [%s]\n",argv[i+7]);
			printf("Press any key to continue\n");
			int j; scanf("%d",&j);
			exit(-1);
		}
		neighbors[i].port = neighborPort;

		/* validates neighbor's port */
		int neighborCost;
		int rv = sscanf(argv[i+8],"%u",&neighborCost);
		if(neighborCost < 1){
			fprintf(stderr, "invalid neighbor's cost [%s]\n",argv[i+8]);
			printf("Press any key to continue\n");
			int j; scanf("%d",&j);
			exit(-1);
		}
		neighbors[i].cost = neighborCost;
	}

	free(neighbors);
	exit(0);
}

