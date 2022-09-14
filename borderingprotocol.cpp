#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <map>
#include "api.h"
#include "network_ip.h"
#include "partition.h"
#include "mapping.h"
#include "borderingprotocol.h"

using namespace std;


void 
BorderingHandleProtocolEvent(Node * node, Message * msg)
{
	switch(msg->eventType)
	{
		case MSG_SEND_ANCHORADV:
		{
			BorderingSendAnchorAdvAndScheduleNext(node);
			break;
		}
		case MSG_SEND_BORDER_NOTIFICATION:
		{	
			BorderingSendBorderNotificationAndScheduleNext( node);
			break;
		}
		case MSG_CHECK_ANCHOR_VALIDITY:
		{
			BorderingHandleAnchorStatusCheckingEvent(node);
			break;
		}
		case MSG_CHECK_BORDER_VALIDITY:
		{
			BorderingHandleBorderStatusCheckingEvent(node);
			break;
		}
		case MSG_CHECK_BORDER_DB_VALIDITY:
		{
			//Bang: I need to now how to insert border desired border IP Address in the message
			NodeAddress* info= (NodeAddress*)MESSAGE_ReturnInfo(msg,INFO_TYPE_BorderIpAddress);
			
			BorderingHandleBorderDBUpdateEvent(node,*info);
			break;
		}
		case MSG_SEND_MCAST_REQUEST:
		{
			NodeAddress *mcastAddress = (NodeAddress*)MESSAGE_ReturnInfo(msg,INFO_TYPE_McastAddress);
			BorderingSendMcastRequestAndScheduleNext(node,*mcastAddress);
			break;
		}
		case MSG_CHECK_MCAST_MEMBERSHIP:
		{
			BorderingHandleMcastMemebershipTimer(node,msg);
			break;
		}
		default:
            {
		char buf[100];
                sprintf(buf,
                    "Node %u received message of unknown\n"
                    "type %d.\n",
                    node->nodeId, msg->eventType);
                ERROR_Assert(FALSE, buf);
            }
	}
	MESSAGE_Free(node,msg);
}

void 
BorderingSendAnchorAdvAndScheduleNext(Node* node)
{
	Message* newMsg;
	AnchorAdv option;
	NodeAddress nodeIPAddress;
	clocktype delay=ANCHOR_ADVERTISEMENT_INTERVAL*SECOND;

	newMsg = MESSAGE_Alloc(node,MAC_LAYER,0,MSG_MAC_FromNetwork);
	nodeIPAddress = NetworkIpGetInterfaceAddress(node,
                                                 DEFAULT_INTERFACE);
	option.anchorAddress = nodeIPAddress;
	option.anchorId = node->nodeId;
	MESSAGE_PacketAlloc(node,newMsg,sizeof(AnchorAdv),TRACE_IP);
	memcpy(newMsg->packet, (char*)(&option), sizeof(AnchorAdv));
	unsigned char ipProtocolNumber = IPPROTO_ANCHOR_ADV;
	NetworkIpAddHeader(node,newMsg,nodeIPAddress,
		ANY_DEST,
		IPTOS_PREC_INTERNETCONTROL,
		IPPROTO_ANCHOR_ADV,
		2);
	//printf("%d send IPPROTO %u\n,",node->nodeId, IPPROTO_ANCHOR_ADV);
	NetworkIpSendPacketToMacLayerWithDelay(node,
		newMsg,
		DEFAULT_INTERFACE,
		ANY_DEST,
 		0);
	newMsg=MESSAGE_Alloc(node,
		NETWORK_LAYER,
		NODE_STATUS_MANAGEMENT,
		MSG_SEND_ANCHORADV);
	MESSAGE_Send(node,newMsg,delay);
	
}

void
BorderingSendBorderNotificationAndScheduleNext(Node * node)
{
	Message* newMsg;
	BorderNotification option;
	NodeAddress nodeIPAddress;
	clocktype delay=0;
	NodeAddress leaderIPAddress;
	Node* leader;

	if(!node->border)
		return;
	

	//To the Anchor
	newMsg = MESSAGE_Alloc(node,MAC_LAYER,0,MSG_MAC_FromNetwork);
	nodeIPAddress = NetworkIpGetInterfaceAddress(node,
                                                 DEFAULT_INTERFACE);
	option.anchorAddress = node->anchorIPAddress;
	option.borderAddress = nodeIPAddress;
	option.manetNum = node->groupInfo.groupNum;
	MESSAGE_PacketAlloc(node,newMsg,sizeof(BorderNotification),TRACE_IP);
	memcpy(newMsg->packet, (char*)(&option), sizeof(BorderNotification));
	NetworkIpAddHeader(node,newMsg,nodeIPAddress,
		node->anchorIPAddress,
		IPTOS_PREC_INTERNETCONTROL,
		IPPROTO_BORDER_NOTIFICATION,
		1);
	NetworkIpSendPacketToMacLayerWithDelay(node,
		newMsg,
		DEFAULT_INTERFACE,ANY_DEST,
		0);
	
	//To the leader
	leader = MAPPING_GetNodePtrFromHash(
                               node->partitionData->nodeIdHash,node->groupInfo.leaderId);

	leaderIPAddress = NetworkIpGetInterfaceAddress(leader,
											DEFAULT_INTERFACE);

	newMsg = MESSAGE_Alloc(node,
							MAC_LAYER,0,MSG_MAC_FromNetwork);
	MESSAGE_PacketAlloc(node,newMsg,sizeof(BorderNotification),TRACE_IP);
	memcpy(newMsg->packet, (char*)(&option), sizeof(BorderNotification));
	NetworkIpAddHeader(node,newMsg,nodeIPAddress,
		leaderIPAddress,
		IPTOS_PREC_INTERNETCONTROL,
		IPPROTO_BORDER_NOTIFICATION,
		BORDER_NOTIFICATION_TTL);

	RoutePacketAndSendToMac(node,
								newMsg, 
								CPU_INTERFACE,
								DEFAULT_INTERFACE,
								ANY_IP);
	/*NetworkIpSendPacketToMacLayerWithDelay(node,
		newMsg,
		DEFAULT_INTERFACE,ANY_DEST,
		0);*/

	//Schedule next
	newMsg=MESSAGE_Alloc(node,
		NETWORK_LAYER,
		NODE_STATUS_MANAGEMENT,
		MSG_SEND_BORDER_NOTIFICATION);
	delay = BORDER_NOTIFICATION_INTERVAL*SECOND;
	MESSAGE_Send(node,newMsg,delay);
}

void 
BorderingHandleAnchorAdv(Node * node, Message * msg)
{
	char currentTime[20];
	Message* newMsg;
	clocktype delay;
	if(node->groupInfo.backBone)
	{
		MESSAGE_Free(node,msg);
		return;
	}
	AnchorAdv* advPkt = (AnchorAdv*)MESSAGE_ReturnPacket(msg);
	node->anchorIPAddress =advPkt ->anchorAddress;
	TIME_PrintClockInSecond(getSimTime(node), currentTime);
	node->borderDesignationTime=atof(currentTime);
	if(node->border)
		return;
	else
		{
			node->border = true;
			node->anchorId = advPkt->anchorId;
			/*
			newMsg=MESSAGE_Alloc(node,
					NETWORK_LAYER,
					NODE_STATUS_MANAGEMENT,
					MSG_CHECK_BORDER_VALIDITY);
			delay =  VALIDITY_CHECKING_INTERVAL * SECOND;
			MESSAGE_Send(node,newMsg,delay);*/
			BorderingHandleBorderStatusCheckingEvent(node);
			BorderingSendBorderNotificationAndScheduleNext(node);
		}
	MESSAGE_Free(node,msg);
	
	
}

void
BorderingHandleBorderNotification(Node * node, Message * msg)
{
	BorderNotification* pkt = (BorderNotification*)MESSAGE_ReturnPacket(msg);
	NodeAddress anchorAddress = pkt->anchorAddress;
	NodeAddress borderAddress = pkt->borderAddress;
	std::map<NodeAddress,BorderData>::iterator it;
	char currentTime[20];
	
	TIME_PrintClockInSecond(getSimTime(node), currentTime);
	
	if(node->groupInfo.leader && !node->groupInfo.backBone)
	{
		it=node->borderDB->find(borderAddress);
		if(it!=node->borderDB->end())
		{
			it->second.updateTime = atof(currentTime);
		}
		else
		{
			BorderData borderData;
			borderData.anchorIPAddress = pkt->anchorAddress;
			borderData.updateTime = atof(currentTime);
			node->borderDB->insert
				(std::pair<NodeAddress,BorderData>(pkt->borderAddress,borderData));

			BorderingHandleBorderDBUpdateEvent(node,pkt->borderAddress);
		}
	}
	else if(node->groupInfo.backBone)
	{
		node->anchorDesignationTime =  atof(currentTime);
		node->underlyingManet = pkt->manetNum;
		node->borderAddr = pkt->borderAddress;
		if(!node->anchor)
		{
				node->anchor = true;
				BorderingHandleAnchorStatusCheckingEvent(node);

				//Bang: AnchorStatus checking is needed
				//BorderingHandleBorderStatusCheckingEvent(node);
		}
	}
	MESSAGE_Free(node,msg);
}

//Bang: The Anchor nodes check whether he is valid or not
void
BorderingHandleAnchorStatusCheckingEvent(Node* node)
{
	double unUpdatedDuration;
	char currentTime[20];
	TIME_PrintClockInSecond(getSimTime(node), currentTime);
	unUpdatedDuration=atof(currentTime)-(node->anchorDesignationTime);
	if(unUpdatedDuration >  VALIDITY_CHECKING_INTERVAL)
		node->anchor = false;
	else
	{
		node->anchor = true;
		Message* newMsg=MESSAGE_Alloc(node,
					NETWORK_LAYER,
					NODE_STATUS_MANAGEMENT,
					MSG_CHECK_ANCHOR_VALIDITY);
		clocktype delay =  VALIDITY_CHECKING_INTERVAL * SECOND;
		MESSAGE_Send(node,newMsg,delay);

	}

}
//Bang: The Border node check whether he is valid or not
void
BorderingHandleBorderStatusCheckingEvent(Node * node)
{
	double unUpdatedDuration;
	char currentTime[20];
	TIME_PrintClockInSecond(getSimTime(node), currentTime);
	unUpdatedDuration=atof(currentTime)-(node->borderDesignationTime);
	if(unUpdatedDuration >  VALIDITY_CHECKING_INTERVAL)
		node->border = false;
	else
	{
		node->border = true;

	//schedule next checking event
		Message* newMsg=MESSAGE_Alloc(node,
					NETWORK_LAYER,
					NODE_STATUS_MANAGEMENT,
					MSG_CHECK_BORDER_VALIDITY);
		clocktype delay =  VALIDITY_CHECKING_INTERVAL * SECOND;
		MESSAGE_Send(node,newMsg,delay);
	}
}


//Bang: The leader node checks whether border DB is valid or not
void
BorderingHandleBorderDBUpdateEvent(Node * node, NodeAddress borderIPAddr)
{
	double unUpdatedDuration;
	char currentTime[20];
	NodeAddress BorderIP_temp=borderIPAddr;
	TIME_PrintClockInSecond(getSimTime(node), currentTime);
	NodeAddress* pt;
	Message* newMsg;

	std::map<NodeAddress,BorderData>::iterator it;
	it=node->borderDB->find(borderIPAddr);

	unUpdatedDuration =atof(currentTime)-(it->second.updateTime);
	if(unUpdatedDuration > VALIDITY_CHECKING_INTERVAL)
	{
		node->borderDB->erase(borderIPAddr);
		return;
	}
	else
	{
		Message* newMsg=MESSAGE_Alloc(node,
										NETWORK_LAYER,
										NODE_STATUS_MANAGEMENT,
										MSG_CHECK_BORDER_DB_VALIDITY);
		
		pt= (NodeAddress*)MESSAGE_AddInfo(node,
							newMsg,
							sizeof(NodeAddress),
							INFO_TYPE_BorderIpAddress);
		memcpy((void*)pt,(void*)&BorderIP_temp,sizeof(NodeAddress));
		
		
		clocktype delay= VALIDITY_CHECKING_INTERVAL*SECOND;
		MESSAGE_Send(node,newMsg,delay);

		
	}
}

void
BorderingSendMcastRequestAndScheduleNext(Node * node, NodeAddress mcastAddr)
{
	Message* newMsg;

	if(!NetworkIpIsPartOfMulticastGroup(node,mcastAddr))
	{
		return;
	}

	Node* leader = MAPPING_GetNodePtrFromHash(
                               node->partitionData->nodeIdHash,node->groupInfo.leaderId);
	NodeAddress AnchorAddr;
	std::map<NodeAddress,BorderData>::iterator it;
	
	if(leader->borderDB->size()!=0)
	{
		it = leader->borderDB->begin();
		BorderData borderData = it->second;
		AnchorAddr = borderData.anchorIPAddress;

		char anchorIpString[40];
		IO_ConvertIpAddressToString(AnchorAddr, anchorIpString);

		NodeAddress leaderIPAddress = NetworkIpGetInterfaceAddress(leader,
												DEFAULT_INTERFACE);

		newMsg = MESSAGE_Alloc(node,MAC_LAYER,0,MSG_MAC_FromNetwork);
		McastRequest option;
		NodeAddress nodeIPAddress = NetworkIpGetInterfaceAddress(node,
													 DEFAULT_INTERFACE);
		
		option.mcastAddr = mcastAddr;
		MESSAGE_PacketAlloc(node,newMsg,sizeof(McastRequest),TRACE_IP);
		memcpy(newMsg->packet, (char*)(&option), sizeof(McastRequest));
		NetworkIpAddHeader(node,
			newMsg,
			nodeIPAddress,
			AnchorAddr,
			IPTOS_PREC_INTERNETCONTROL,
			IPPROTO_MCAST_REQUEST,
			BORDER_NOTIFICATION_TTL);

		RoutePacketAndSendToMac(node,
									newMsg, 
									CPU_INTERFACE,
									DEFAULT_INTERFACE,
									ANY_IP);
	}
	
	newMsg=MESSAGE_Alloc(node,
							NETWORK_LAYER,
							NODE_STATUS_MANAGEMENT,
							MSG_SEND_MCAST_REQUEST);
	NodeAddress* pt= (NodeAddress*)MESSAGE_AddInfo(node,
							newMsg,
							sizeof(NodeAddress),
							INFO_TYPE_McastAddress);
	memcpy((void*)pt,(void*)&mcastAddr,sizeof(NodeAddress));
	clocktype delay = BORDER_NOTIFICATION_INTERVAL*SECOND;
	MESSAGE_Send(node,newMsg,delay);
	
	
}


//Bang: For the multicast router to handle the multicast request sent from underlying MANET
void 
BorderingHandleMcastRequest(Node* node, Message* msg)
{
	char currentTime[20];
	Message* timer_Msg;
	clocktype delay;
	double receivedTime;
	NodeAddress mcastAddr;
	std::map<NodeAddress,double>::iterator it;
	if(!node->groupInfo.backBone)
	{
		MESSAGE_Free(node,msg);
		return;
	}
	McastRequest* pkt = (McastRequest*)MESSAGE_ReturnPacket(msg);
	TIME_PrintClockInSecond(getSimTime(node), currentTime);
	receivedTime=atof(currentTime);
	mcastAddr = pkt->mcastAddr;
	it=node->mcastDB->find(mcastAddr);
	if(it==node->mcastDB->end())
	{
		node->mcastDB->insert(std::pair<NodeAddress,double>(mcastAddr,receivedTime));

		//Bang: Schedule timer message
		timer_Msg = MESSAGE_Alloc(node,
								NETWORK_LAYER,
								NODE_STATUS_MANAGEMENT,
								MSG_CHECK_MCAST_MEMBERSHIP);
		NodeAddress* pt= (NodeAddress*)MESSAGE_AddInfo(node,
								timer_Msg,
								sizeof(NodeAddress),
								INFO_TYPE_McastAddress);
		memcpy((void*)pt,(void*)&(mcastAddr),sizeof(NodeAddress));
		delay = VALIDITY_CHECKING_INTERVAL*SECOND;
		MESSAGE_Send(node,timer_Msg,delay);
	}
	else
	{
		it->second = receivedTime;
	}
	MulticastProtocolType joinOrLeaveFunction = node->joinOrLeaveFunction;
	if(joinOrLeaveFunction==NULL)
	{
		printf("JoinOrLeaveFunction is NULL");
	}
	 (joinOrLeaveFunction)(node,
							mcastAddr,
						 DEFAULT_INTERFACE,
						 LOCAL_MEMBER_JOIN_GROUP);
	
	MESSAGE_Free(node,msg);
}


void
BorderingHandleMcastMemebershipTimer(Node* node, Message* msg)
{
	//Gets multicast address of defined in the message
	NodeAddress *mcastAddress =
		(NodeAddress*)MESSAGE_ReturnInfo(msg,INFO_TYPE_McastAddress);
	//Finds data at the McastDB
	std::map<NodeAddress,double>::iterator it;
	char currentTime[20];
	double lastReceived;
	TIME_PrintClockInSecond(getSimTime(node), currentTime);
	double now=atof(currentTime);
		
	it=node->mcastDB->find(*mcastAddress);
	if(it!=node->mcastDB->end())
	{
		  //Calculate time duration that the router hasn't received mcast request
		  //if that's larger than predefined time, delete it from the mcastDB
		if(now - lastReceived > VALIDITY_CHECKING_INTERVAL)
		{
		  //send leave to multicast routing protocol
		  MulticastProtocolType joinOrLeaveFunction = node->joinOrLeaveFunction;
		  (joinOrLeaveFunction)(node,
                                     *mcastAddress,
                                     DEFAULT_INTERFACE,
                                     LOCAL_MEMBER_LEAVE_GROUP);
		  //delete from mcastDB
		  node->mcastDB->erase(it);
		}
	 }
}


