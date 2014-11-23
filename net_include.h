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

struct chat_packet {
  int type;//Type of packet 0 for message, 1 for like
  int sequence; //Sequence of message given to the server
  int server_id; //Server id that the message was created on.
  char name[25]; //Text name of user
  char group[25]; //Text name of chat room
  char text[80]; //Text of chat message
  int like_sequence; //Integer of sequence number of message liked
  int resend;
};

