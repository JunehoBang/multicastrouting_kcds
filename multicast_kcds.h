#ifndef _KCDS_H_
#define _KCDS_H_

#include <stdio.h>
#include <stdlib.h>
#include <list>
#include "api.h" 
#include "network_ip.h"

#include <iostream>
#include <set>

using namespace std;



typedef std::pair<NodeAddress,int>	CacheElement;
typedef std::set<CacheElement> MsgCache;

typedef struct struct_routing_kcds_str
{
	MsgCache* messageCache;
	int sendSeqNo;
} KcdsData;

class Graph
{
	private:
		int V;
		std::list<int> *adj;
		//set<int> *kcds;
	public:
		Graph(int V)
		{
			this->V = V;
			adj = new list<int>[V];
		}
		
		void addEdge(int v, int w);
		void BFS(int s, bool visited[]);
		Graph getTranspose();
		bool isConnected();
};
/*
typedef struct
{
    int seqNumber;
    unsigned int protocol;
    int prevNode;
   //double sentTime;
} KcdsIpOptionType;*/

//comes from Network_ip.cpp >> IpRoutingInit()
void KcdsInit(Node* node, int interfaceIndex); //!!
void KcdsHandleProtocolEvent(Node* node, Message* msg); //!!
//comes from Network_ip.cpp >> RoutePacketandSendToMac() [16726]
void KcdsRouterFunction(Node* node,
                          Message* msg,
                          NodeAddress destAddr,
                          int interfaceIndex,
                          BOOL* packetWasRouted,
                          NodeAddress prevHop); 

//comes from KcdsHandleProtocolEvent()
void KcdsCalculateKcds(Node* node); 

bool KcdsLookupMessageCache(Node* node, NodeAddress mcastAddr, int seqNo);
void KcdsInsertMessageCache(Node* node, NodeAddress mcastAddr, int seqNo);
void KcdsSendData(KcdsData* kcdsData, Node* node, Message* msg, 
						NodeAddress mcastAddr);

void KcdsHandleData(Node* node, Message* msg, int incommingInterface);
bool KcdsAmIRelay(Node* node);




void cga(int** adj_g, int g_size,int k,set<int>* kcds);
void induce(int** adj_h, int** adj_g, set<int> *subset);
bool connectivity(int** adj_g,int g_size, int con, set<int> remover,int starter=0);
int dominativity(int** adj_g,int g_size, set<int> *kcds);
int dominativity(int** adj_g,int g_size, set<int> *kcds, int node);
bool connectedness(int** adj_g,int g_size);
int nodedegree(int** adj_g,int g_size,int node);
void sortnodebydegree(int**adj_g, int g_size, set<int> *subset, int* nodelist);
bool retirementcondition(int **adj_g,int g_size,int k, set<int> *kcds, int retirecandidate);

#endif

