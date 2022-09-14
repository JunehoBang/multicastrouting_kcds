
#include <stdio.h>
#include <stdlib.h>
#include "api.h" 
#include "network_ip.h"
#include "mapping.h"
#include <math.h>

#include <iostream>
#include <set>
#include "multicast_kcds.h"
#include "partition.h"

#define SIGNAL_DISTANCE 100
#define K_VALUE 2
#define KCDS_CALCULATE_INTERVAL 3

#define X_AXIS	0
#define Y_AXIS	1


void KcdsInit(Node* node, int interfaceIndex)
{
	KcdsData* kcdsData=new KcdsData();
	kcdsData->sendSeqNo=0;
	kcdsData->messageCache=new MsgCache();

	NetworkIpSetMulticastRoutingProtocol(node, kcdsData, interfaceIndex);
	NetworkIpSetMulticastRouterFunction(node,&KcdsRouterFunction,interfaceIndex);

	if(node->groupInfo.leader)
	{
		node->groupInfo.kcdsNodes=new KcdsSet();
		Message* kcdsInitMessage=MESSAGE_Alloc(node, 
						NETWORK_LAYER, 
						MULTICAST_PROTOCOL_KCDS,
						MSG_CALCULATE_KCDS);
		clocktype delay=5*MICRO_SECOND;
		MESSAGE_Send(node, kcdsInitMessage,delay);
	}
	//no need to start registering position
}

void KcdsRouterFunction(Node* node,
                          Message* msg,
                          NodeAddress destAddr,
                          int interfaceIndex,
                          BOOL* packetWasRouted,
                          NodeAddress prevHop)
{
	*packetWasRouted = true;
	int i;
	IpHeaderType* ipHeader = (IpHeaderType *) msg->packet;
	unsigned int originalProtocol = (unsigned int) ipHeader->ip_p;
	KcdsData* kcdsData = (KcdsData*)
		          NetworkIpGetMulticastRoutingProtocol(node, MULTICAST_PROTOCOL_KCDS);
	bool sender=false;

	//if i'm the sender
	
	for(i=0;i<node->numberInterfaces;i++)
	{
		if( NetworkIpGetInterfaceAddress(node,i)==ipHeader->ip_src)
		{
			sender=true;
			break;
		}
	}

	if(prevHop==ANY_IP)
	{
		KcdsSendData(kcdsData, node, msg, destAddr);
	}

	else
	{
		KcdsHandleData(node, msg,interfaceIndex);
	}
 	
	

}

void KcdsHandleProtocolEvent(Node* node, Message* msg)
{
	clocktype delay;
	KcdsData* kcdsData = (KcdsData*)
		          NetworkIpGetMulticastRoutingProtocol(node, MULTICAST_PROTOCOL_KCDS);
	switch(msg->eventType)
		{
			case MSG_CALCULATE_KCDS:
			{
				KcdsCalculateKcds(node);
				//Schedule next calculation
				Message* kcdsInitMessage=MESSAGE_Alloc(node, 
					NETWORK_LAYER, 
					MULTICAST_PROTOCOL_KCDS,
					MSG_CALCULATE_KCDS);
				delay=KCDS_CALCULATE_INTERVAL*SECOND;
				MESSAGE_Send(node, kcdsInitMessage,delay);
				break;
			}
			default:
			{
				char buf[200];
				sprintf(buf,   "Node %u received message of unknown\n"
			                    "type %d.\n",
			                    node->nodeId, msg->eventType);
            			ERROR_Assert(FALSE, buf);
			}
		}
	MESSAGE_Free(node,msg);
	//The messge should be freed from here.
}

void KcdsCalculateKcds(Node* node)
{
	std::set<int> kcds;
	std::set<int>::iterator it;
	int mapping[100];
	double location[100][2];
	int i;
	int j;
	int temp;
	int groupSize;
	int** adj_g;
	double distance;
	double x_distance;
	double y_distance;
	Node* nodePtr;
	FILE* fp;
	
	groupSize=node->groupInfo.groupSize;
	i=0;
	for(it=node->groupInfo.groupNodes->begin();it!=node->groupInfo.groupNodes->end();it++)
	{
		mapping[i]= *it;
		
		nodePtr = MAPPING_GetNodePtrFromHash(node->partitionData->nodeIdHash,*it);
		location[i][X_AXIS]=nodePtr->mobilityData->current->position.common.c1;
		location[i][Y_AXIS]=nodePtr->mobilityData->current->position.common.c2;
		i++;
	}

	//Bang: making adjacent matrix of 

	if(node->groupInfo.adj_g!=NULL && node->groupInfo.backBone)
	{
		adj_g=node->groupInfo.adj_g;
	}
	else
	{
		adj_g=(int**)malloc(sizeof(int*)*groupSize);
		for(i=0;i<groupSize;i++)
			adj_g[i]=(int*)malloc(sizeof(int)*groupSize);

		for(i=0;i<groupSize;i++)
		{
			for(j=i;j<groupSize;j++)
			{
				if(i==j)
					adj_g[i][j]=0;
				else
				{
					x_distance= location[i][X_AXIS]-location[j][X_AXIS];
					y_distance= location[i][Y_AXIS]-location[j][Y_AXIS];
					distance=pow(x_distance,2)+pow(y_distance,2);
					distance=sqrt(distance);
					
					if(distance<SIGNAL_DISTANCE)
					{
						adj_g[i][j]=1;
						adj_g[j][i]=1;
						//printf("1 ");
					}
					else
					{
						adj_g[i][j]=0;
						adj_g[j][i]=0;
						//printf("0 ");
					}					
				}
			}
			//printf("\n");
		}
	}

	fp=fopen("adj.txt","w");
	for(i=0;i<groupSize;i++)
	{
		for(j=0;j<groupSize;j++)
		{
			//printf("%d ",adj_g[i][j]);
			fprintf(fp,"%d",adj_g[i][j]);
		}
		//printf("\n");
		fprintf(fp,"\n");
	}
	fclose(fp);


	cga(adj_g,groupSize,K_VALUE,&kcds);
	node->groupInfo.kcdsNodes->clear();
	for(it=kcds.begin();it!=kcds.end();it++)
	{
		temp=mapping[*it];
		node->groupInfo.kcdsNodes->insert(temp);
	}
	
	for(i=0;i<groupSize;i++)
		free(adj_g[i]);
	free(adj_g);
	
	
}

void KcdsInsertMessageCache(Node* node, NodeAddress mcastAddr, int seqNo)
{
	KcdsData*  kcdsData = (KcdsData*)
		          NetworkIpGetMulticastRoutingProtocol(node, MULTICAST_PROTOCOL_KCDS);
	//unsigned long long  input;
	CacheElement element=std::make_pair (mcastAddr,seqNo);
	kcdsData->messageCache->insert(element);
	return;
}

bool KcdsLookupMessageCache(Node* node, NodeAddress mcastAddr, int seqNo)
{
	KcdsData*  kcdsData = (KcdsData*)
		          NetworkIpGetMulticastRoutingProtocol(node, MULTICAST_PROTOCOL_KCDS);
//	unsigned long long  input;
//	input =  mcastAddr*pow((float)2,(float)32)+seqNo;
	CacheElement element;
	element=std::make_pair(mcastAddr,seqNo);
	if(kcdsData->messageCache->find(element)!=kcdsData->messageCache->end())
		return true; //there's cache entry
	else
		return false; //there's no such cache entry
}

void KcdsSendData(KcdsData* kcdsData, Node* node, Message* msg, 
						NodeAddress mcastAddr)
{
	IpHeaderType* ipHeader =  0;
	int i;
	Message* newMsg;
	clocktype randDelay= 10*MILLI_SECOND;
	MESSAGE_SetLayer(msg, MAC_LAYER, 0);
    	MESSAGE_SetEvent(msg, MSG_MAC_FromNetwork);
	ipHeader = (IpHeaderType *)MESSAGE_ReturnPacket(msg);

	//KcdsIpOptionType option;
	//option.prevNode=node->nodeId;
	//option.protocol=originalProtocol;
	//option.seqNumber = kcdsData->sendSeqNo;
	//kcdsData->sendSeqNo++;
	
	//ipHeader->prevHop =node->nodeId;	
	//ipHeader->seqNo = kcdsData->sendSeqNo;	
	kcdsData->sendSeqNo++;

	//set kcds protocol specific option field of the packet
	if(node->groupInfo.backBone)
	{
		for (i=1;i<node->numberInterfaces;i++)
		{
			
			newMsg = MESSAGE_Duplicate(node, msg);
			ipHeader = (IpHeaderType *)MESSAGE_ReturnPacket(newMsg);
			//ipHeader->prevHop=node->nodeId;
			NetworkIpSendPacketToMacLayerWithDelay(node,
	       								 newMsg,
	        								 i,
	        								 ANY_IP,
	       								 randDelay);
		

	/*
			NetworkIpSendPacketToMacLayerWithDelay(node,
	        						newMsg,
	        						i,
	        						ANY_DEST,
	       						 randDelay);*/
		}
	}
	else
	{
		newMsg = MESSAGE_Duplicate(node, msg);
		ipHeader = (IpHeaderType *)MESSAGE_ReturnPacket(newMsg);
		//ipHeader->prevHop=node->nodeId;
		NetworkIpSendPacketToMacLayerWithDelay(node,
	       								 newMsg,
	        								 DEFAULT_INTERFACE,
	        								 ANY_DEST,
	       								 randDelay);
	}

	KcdsInsertMessageCache( node, mcastAddr,ipHeader->seqNo);
	MESSAGE_Free(node, msg);
}


void KcdsHandleData(Node* node, Message* msg, int incommingInterface)
{
	KcdsData* kcdsData = (KcdsData*)
		          NetworkIpGetMulticastRoutingProtocol(node, MULTICAST_PROTOCOL_KCDS);
	IpHeaderType* ipHeader = (IpHeaderType *)MESSAGE_ReturnPacket(msg);
	NodeAddress                 sourceAddress;
    	NodeAddress                 destinationAddress;
	unsigned char               IpProtocol;
	unsigned int                ttl = 0;
	TosType priority;
	clocktype    delay=10*MILLI_SECOND;
	NodeAddress srcAddr   = ipHeader->ip_src;
	NodeAddress mcastAddr = ipHeader->ip_dst;
	int seqNo = ipHeader->ip_id;
	Message* newMsg = NULL;
	int i;
	

	if(!KcdsLookupMessageCache(node, mcastAddr,seqNo))
	{
		KcdsInsertMessageCache(node, mcastAddr, seqNo);

		//
		if(KcdsAmIRelay(node) 
		    || incommingInterface == CPU_INTERFACE  //If I am sender 
		    || (ipHeader->senderGroup == node->groupInfo.groupNum && node->border) //If I am bordering to sender anchor
			|| (node->border && ipHeader->prevHop == node->anchorId))//If I am bordering to receiver anchor
		{
			if(node->groupInfo.backBone)
			{
				//If I am anchor let's check whether underlying network has receiver of it
				if(node->anchor)  
				{
					std::map<NodeAddress,double>::iterator it;
					it = node->mcastDB->find(mcastAddr);
					if(it!=node->mcastDB->end())
					{
						newMsg = MESSAGE_Duplicate(node, msg);
						NetworkIpSendPacketToMacLayerWithDelay(node,
		               								 newMsg,
		                								 DEFAULT_INTERFACE,
		                								 ANY_DEST,
		               								 delay);
					}
				}
				
				for(i=1;i<node->numberInterfaces;i++)
				{
					if(i!=incommingInterface)
					{
						newMsg = MESSAGE_Duplicate(node, msg);
						ipHeader = (IpHeaderType *)MESSAGE_ReturnPacket(newMsg);
						//ipHeader->prevHop=node->nodeId;
						NetworkIpSendPacketToMacLayerWithDelay(node,
		               								 newMsg,
		                								 i,
		                								 ANY_DEST,
		               								 delay);
					}
	
				}
				
			}
			else //I'm MANET node
			{
				newMsg = MESSAGE_Duplicate(node, msg);
				ipHeader = (IpHeaderType *)MESSAGE_ReturnPacket(newMsg);
				//ipHeader->prevHop=node->nodeId;
				NetworkIpSendPacketToMacLayerWithDelay(node,
		               								 newMsg,
		                								 DEFAULT_INTERFACE,
		                								 ANY_DEST,
		               								 delay);
				
			}
				
			MESSAGE_Free(node,msg);
			
		}
		else
		{
			MESSAGE_Free(node,msg);
		}
		
	}
	else
	{
		MESSAGE_Free(node,msg);
	}
	
}

bool KcdsAmIRelay(Node* node)
{
	Node* leader = MAPPING_GetNodePtrFromHash(
                               node->partitionData->nodeIdHash,node->groupInfo.leaderId);
	if(leader->groupInfo.kcdsNodes->find(node->nodeId)
		                !=leader->groupInfo.kcdsNodes->end())
		return true;
	else
		return false;
		
}


//cga calculation related functions

void Graph::addEdge(int v, int w)

{
	adj[v].push_back(w);
	adj[w].push_back(v);
}

//  A recursive function to print BFS starting from s

void Graph::BFS(int s, bool visited[])

{

	std::list<int> q;
	std::list<int>::iterator i;
	visited[s] = true;
	q.push_back(s);
	while (!q.empty())
	{
		s = q.front();
		q.pop_front();
		for(i = adj[s].begin(); i != adj[s].end(); ++i)
		{
			if(!visited[*i])
			{
				 visited[*i] = true;
				 q.push_back(*i);
			}
		}
		
	}
}

Graph Graph::getTranspose()
{

	Graph g(V);
	for (int v = 0; v < V; v++)
	{
		list<int>::iterator i;
		for(i = adj[v].begin(); i != adj[v].end(); ++i)
		{
			g.adj[*i].push_back(v);
		}
	}
	return g;
}


//Check if Graph is Connected


bool Graph::isConnected()
{
	bool *visited;
	//bang: needs free
	visited = (bool*) malloc(this->V * sizeof(bool));
	for (int i = 0; i < V; i++)
		visited[i] = false;

	BFS(0, visited);

	for (int i = 0; i < V; i++)
		if (visited[i] == false)
		{
			//needs free
			return false;
		}
	Graph gr = getTranspose();

	for(int i = 0; i < V; i++)
		visited[i] = false;

	gr.BFS(0, visited);

	for (int i = 0; i < V; i++)
		if (visited[i] == false)
		{
			//needs free
			return false;
		}
	
	free(visited);
	return true;
}


bool connectedness(int** adj_g,int g_size)
{
	int nv;
	nv=g_size;
	int i,j;
	bool result;
	Graph g(nv);
	
	for(i=0;i<nv;i++)
	{
		for(j=i+1;j<nv;j++)
		{
			if(adj_g[i][j]==1)
				g.addEdge(i,j);
		}
	}
	result=g.isConnected();
	return result;
}



void cga(int** adj_g, int g_size,int k,set<int>* kcds)
{
	int *nodelist,*kcdslist;
	int i,j,l;
	int **adj_h;
	nodelist=(int*)malloc(sizeof(int)*g_size);
	std::set<int> separatingSet,subset;						//dummy separating set;
	std::set<int> psuedokcds;
	for(i=0;i<g_size;i++)
		subset.insert(i);
	sortnodebydegree(adj_g,g_size,&subset,nodelist);
	//for(i=0;i<g_size;i++)
	//	printf("%d\n",nodelist[i]);
	for(i=k;i>0;i=i-1)
	{
		if(connectivity(adj_g,g_size,i,separatingSet)==true)
		{
			break;
		}

	}
	k=i;
	if(k==0)
		*kcds=subset;
	//1) Dominativity
	for(i=0;i<g_size;i++)
	{
		if(dominativity(adj_g,g_size,kcds,nodelist[i])<k)
		{
			kcds->insert(nodelist[i]);
		}
	}		
	//2) Connectivity
	int tempkcdssize;
	for(i=0;i<g_size;i++)
	{
		tempkcdssize=kcds->size();
		adj_h=(int**)malloc(tempkcdssize*sizeof(int*));
		for(j=0;j<tempkcdssize;j++)
		{
			adj_h[j]=(int*)malloc(kcds->size()*sizeof(int));
		}
		
		induce(adj_h,adj_g,kcds);
		/*
		printf("\n");
		for(j=0;j<tempkcdssize;j++)
		{
			for(l=0;l<tempkcdssize;l++)
			{
				printf("%d ",adj_h[j][l]);
			}
			printf("\n");
		}*/
		
		if(connectivity(adj_h,tempkcdssize,k,separatingSet)==false)	//dummy separating set is used
		{	
			kcds->insert(nodelist[i]);
		}
		else
		{
			break;
		}
		
		//freeing adj_h memory
		for(j=0;j<tempkcdssize;j++)
		{
			free(adj_h[j]);
		}
		free(adj_h);

	}
	free(nodelist);
	nodelist=(int*)malloc(sizeof(int)*kcds->size());
	kcdslist=(int*)malloc(sizeof(int)*kcds->size());

	//3) Optimization
	//sort node by degree
	sortnodebydegree(adj_g,g_size,kcds,nodelist);
	//reverse the sort
	for(i=0;i<kcds->size();i++)
	{
		kcdslist[i]=nodelist[kcds->size()-i-1];
		//printf("%d %d\n",kcdslist[i],nodedegree(adj_g,g_size,kcdslist[i]));
	}
	std::set<int> dominated;
	tempkcdssize=kcds->size();
	for(i=0;i<tempkcdssize;i++)
	{
		if(retirementcondition(adj_g,g_size,k,kcds,kcdslist[i]))
			kcds->erase(kcdslist[i]);
	}
	return;
}

bool connectivity(int** adj_g,int g_size, int con, set<int> remover,int starter)
{
	int i,j;
	int nv;
	int subsetSize;
	bool result;
	nv=g_size;
	set<int> subset;

	if(remover.size()==con-1)
	{
		//induce the graph which consists of nodes without removers
		subsetSize=nv-con+1;
		int **adj_h;
		adj_h=(int**)malloc(subsetSize*sizeof(int*));
		for(j=0;j<subsetSize;j++)
			adj_h[j]=(int*)malloc(subsetSize*sizeof(int));

		for(j=0;j<nv;j++)
		{
			if(remover.find(j)==remover.end())
			{
				subset.insert(j);
			}
		}
		induce(adj_h,adj_g,&subset);		
		//test connectedness
		result=connectedness(adj_h,subset.size());
		//if false, return false
		//needs free
		for(i=0;i<subsetSize;i++)
		{
			free(adj_h[i]);
		}
		free(adj_h);
		return result;
	}

	else
	{
		for(i=starter;i<nv;i++)
		{
			remover.insert(i);
			result=connectivity(adj_g,g_size,con,remover,i+1);
			if(result==false)
			{
				return false;
			}
			remover.erase(i);
		}
	return true;
	}
	
}


void induce(int **adj_h,int **adj_g, set<int> *subset)
{
	int i,j;
	std::set<int>::iterator it1, it2;
	//adj_h=(int**)malloc(sizeof(int)*subset->size());


	i=0;
	j=0;
	for (it1= subset->begin();it1!=subset->end();it1++)
	{
		j=0;
		for(it2=subset->begin();it2!=subset->end();it2++)
		{
			/*if(i==1&&j==2)
				printf("@@");*/
			adj_h[i][j] = adj_g[*it1][*it2];
			j++;
		}
		i++;
	}
	return;
}

int dominativity(int** adj_g,int g_size, set<int> *kcds)
{
	int gsize=g_size;
	int i;
	int dominativity;
	int count;
	dominativity=gsize;
	std::set<int>::iterator it;



	
	for(i=0;i<gsize;i++)
	{
		count=0;
		if(kcds->find(i)==kcds->end())  //kcds노드가 아니면
		{
			for(it=kcds->begin();it!=kcds->end();it++) //kcds 노드를 따라 돌아가면서
			{
				count=count+adj_g[i][*it];   //더하기
			}
			if(dominativity<count)
				dominativity=count;
		}
	}
	return dominativity;
}

int dominativity(int** adj_g,int g_size, set<int> *kcds, int node)
{
	int gsize=g_size;
	int dominativity;
	int count=0;
	dominativity=gsize;
	std::set<int>::iterator it;



	//Bang: 다시 짜기

	for(it=kcds->begin();it!=kcds->end();it++) //kcds 노드를 따라 돌아가면서
	{
		count = count+adj_g[*it][node];   //더하기
	}

	return count;
}


int nodedegree(int** adj_g,int g_size,int node)
{
	int i;
	int count=0;
	for (i=0;i<g_size;i++)
	{
		count=count+adj_g[node][i];
	}
	return count;
}

void sortnodebydegree(int** adj_g,int g_size,std::set<int> *subset,int* nodelist)	//최초 내림차순
{
	
	int temp[2];
	//int abc;
	int i,j;
	int nodes[500][2];
	set<int>::iterator it;
	/*
	nodes=(int**)malloc(sizeof(int*)*30);
	for(i=0;i<30;i++)
	{	
		nodes[i]=(int*)malloc(sizeof(int)*2);
	}
	nodes[0][0]=1;
	nodes[0][1]=2;
	nodes[1][0]=3;
	nodes[1][1]=4;
	nodes[2][0]=5;
	nodes[2][1]=6;
	nodes[2][0]=7;
	nodes[2][1]=8;

	*/
	i=0;
	for(it=subset->begin();it!=subset->end();it++)
	{
		nodes[i][0]=*it;
		nodes[i][1]=nodedegree(adj_g,g_size,*it);
		i++;
	}
	
	for(i=0;i<subset->size()-1;i++)
	{
		for(j=0;j<subset->size()-1;j++)
		{
			if(nodes[j][1]<nodes[j+1][1])
			{
				temp[0]=nodes[j][0];
				temp[1]=nodes[j][1];
				nodes[j][0]=nodes[j+1][0];
				nodes[j][1]=nodes[j+1][1];
				nodes[j+1][0]=temp[0];
				nodes[j+1][1]=temp[1];
			}

		}

	}

	for(i=0;i<subset->size();i++)
		nodelist[i]=nodes[i][0];

	//for(i=0;i<subset->size();i++)
	//	free(nodes[i]);
	//free(nodes);;
	return;


		
}

bool retirementcondition(int **adj_g,int g_size,int k, set<int> *kcds, int retirecandidate)
{
	int i,j;
	int dom;
	std::set<int> dominated;
	std::set<int> afterretire;
	std::set<int> remover;
	set<int>::iterator it;
	int **adj_h;
	bool result=true;

	afterretire = *kcds;
	afterretire.erase(retirecandidate);
	
	/*
	if(retirecandidate==24)
	{printf("\n");}
	*/

	if(dominativity(adj_g,g_size,&afterretire,retirecandidate)<k)
		return false;
	
	for(i=0;i<g_size;i++)
		if(adj_g[retirecandidate][i]==1)
			dominated.insert(i);
	for(it=kcds->begin();it!=kcds->end();it++)
		dominated.erase(*it);



	for(it=dominated.begin();it!=dominated.end();it++)
	{
			
		dom=dominativity(adj_g,g_size,&afterretire,*it);

		if(dom<=k)
		{
			return false;
			
		}
	}
	

	adj_h=(int**)malloc(sizeof(int*)*afterretire.size());
	for(i=0;i<afterretire.size();i++)
		adj_h[i]=(int*)malloc(sizeof(int)*afterretire.size());

	induce(adj_h,adj_g,&afterretire);


	result=connectivity(adj_h,afterretire.size(),k,remover);
	

	for(i=0;i<afterretire.size();i++)
		free(adj_h[i]);
	free(adj_h);
	return result;


}

