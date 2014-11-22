#include "net_include.h"
/* global variables */
static  char    User[80];
static  char    Spread_name[80];
static  char    Private_group[MAX_GROUP_NAME];
static  mailbox Mbox;
static  int     Num_sent;
static  unsigned int    Previous_len;
static  int     To_exit = 0;

read_disk();
send_vector();
recv_update();
recv_client_msg();
write_data();
memb_change();

void main(int argc, char **argv) 
{
  int server;
  char messages[5];
  if (argc != 2)
  { 
     printf("Usage: chat_server <server id (1-5)>\n");
     exit (0);
  }
  server = atoi(argv[1]);
  printf("Chat Server %u running\n");
}
