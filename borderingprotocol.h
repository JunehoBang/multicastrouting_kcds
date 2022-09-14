#include <stdio.h>
#include <stdlib.h>
#include "api.h"
#include "network_ip.h"
#include <iostream>
#include <set>
#include <map>

using namespace std;

#define ANCHOR_ADVERTISEMENT_INTERVAL 2
#define BORDER_NOTIFICATION_INTERVAL 2
#define MCAST_REQUEST_INTERVAL 2
#define VALIDITY_CHECKING_INTERVAL 6
#define BORDER_NOTIFICATION_TTL 10

typedef struct str_anchor_adv{
	NodeAddress anchorAddress;
	int anchorId;
}AnchorAdv;

typedef struct str_mcast_request{
	NodeAddress mcastAddr;
}McastRequest;

typedef struct str_border_notif{
	NodeAddress borderAddress;
	NodeAddress anchorAddress;
	int manetNum;
}BorderNotification;

void
BorderingHandleProtocolEvent(Node* node, Message* msg);

void 
BorderingSendAnchorAdvAndScheduleNext(Node* node);

void
BorderingSendBorderNotificationAndScheduleNext(Node* node);

void
BorderingHandleAnchorAdv(Node* node, Message* msg);

void
BorderingHandleBorderNotification(Node* node, Message* msg);

void
BorderingHandleBorderStatusCheckingEvent(Node* node);

void
BorderingHandleBorderDBUpdateEvent(Node* node,NodeAddress borderIPAddr);

void
BorderingHandleAnchorStatusCheckingEvent(Node* node);

void
BorderingSendMcastRequestAndScheduleNext(Node* node, NodeAddress mcastAddr);

void 
BorderingHandleMcastRequest(Node* node, Message* msg);

void
BorderingHandleMcastMemebershipTimer(Node* node, Message* msg);