#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include "sp.h"
#include "unistd.h"
#include "stdlib.h"

#define PORT	     10080 /* assigned address */
#define MAX_MESSLEN     102400
#define MAX_VSSETS      10
#define MAX_MEMBERS     100

struct likes {
  char name[25];
  struct likes *next;
};

struct chat_packet {
  int type;//Type of packet 0 for message, 1 for like
  int sequence; //Lamport timestamp
  int server_id; //Server id that the message was created on.
  char name[25]; //Text name of user
  char group[25]; //Text name of chat room
  char text[80]; //Text of chat message
  struct likes likes; //Integer of sequence number of message liked
  char client_group[MAX_GROUP_NAME]; //Used to ID client private group so server can respond
  int resend;
};


struct vector {
  int vector[5][5];
};

struct chatrooms {
  char name[25]; //Chat room name
  struct node *head; /* first chat in room */
  struct node *tail; /* Last chat in room */
  struct chatrooms *next; 
};


/*
 *  These are the nodes used in data structure that holds the packets of
 *  all servers.
 */
struct node {
  struct chat_packet* data;
  struct node*        next;
  int    sequence; /*Client line sequence number */
};
