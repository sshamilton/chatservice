#include "net_include.h"
/* global variables */
static  char    User[MAX_PRIVATE_NAME];
static  char    Spread_name[80];
static  char    Private_group[MAX_GROUP_NAME];
static  mailbox Mbox;
static  struct vector*   Vector[5][5]; 
static  int     Num_sent;
static  unsigned int    Previous_len;
static  int     To_exit = 0;
static  struct node     Server_packets[5];
// Essentially the 5 tail nodes of the 5 linked lists.
static  struct node*    Last_packets[5];
// Pointer to last node that has been written to disk
static  struct node*    Last_written;
static  char	server[1];
static  FILE   *fp;
static  int    lsequence = 0;
static struct chatrooms* chatroomhead;

void read_disk();
void send_vector();
void recv_update();
void recv_client_msg();
void write_data();
void memb_change();
/* Message Types */
/* 0 - Display chat text */
/* 1 - Like message */
/* 2 - Connect to server */
/* 3 - Receive message (to memory, don't display) */
/* 4 - Display all lines in chatroom after given lamport timestamp */
/* 5 - Request to joing group */
/* 6 - Refresh client screen */

/*
 * Read disk is used for recovering from crashes. Things to do:
 * 1. Restore data structure and recover data for users connected to the
 * server.
 * 2. Restore the vector.
 */
void read_disk() {
  int                 i, foundroom =0;
  struct chat_packet *c_temp, r_temp;
  c_temp = malloc(sizeof(struct chat_packet));
  struct chatrooms *rooms;
  rooms = chatroomhead;
  while (fread(c_temp, sizeof(struct chat_packet), 1, fp) != 0) {
    i = c_temp->server_id;

    // Allocate memory for the next node.
    Last_packets[i]->next = (struct node *) malloc(sizeof(struct node));

    // Update the n_temp pointer to new tail node.
    Last_packets[i] = Last_packets[i]->next;

    // Allocate memory for the new node's data.
    Last_packets[i]->data = (struct chat_packet *) malloc(sizeof(struct chat_packet));

    // memcpy the read data into the newly allocated memory for the next node.
    memcpy(Last_packets[i]->data, c_temp, sizeof(struct chat_packet));

    while (rooms->next != NULL)
    {
       rooms = rooms->next;
       if (strncmp(rooms->name, c_temp->group, strlen(c_temp->group) == 0))
	{
	  /*chatroom exists*/
	  foundroom = 1;
          rooms->tail = rooms->tail->next;
	  rooms->tail->data = malloc(sizeof(struct chat_packet)); 
	  memcpy(rooms->tail->data, c_temp, sizeof(struct chat_packet));
	}
    }
    if (!foundroom) /*room doesn't exist, so create it */
    {
       rooms->next = malloc(sizeof(struct chatrooms));
       rooms = rooms->next;
       strncpy(rooms->name, c_temp->group, strlen(c_temp->group));
       rooms->head->data = malloc(sizeof(struct chat_packet));
       memcpy(rooms->head->data, c_temp, sizeof(struct chat_packet));

    }
    // Clear the new node's next pointer for safety.
    Last_packets[i]->next = NULL;


    // Update the Last_written to the newly created node.
    Last_written = Last_packets[i];
  }
}
 
/*void send_vector() {
        int ret;
        // Send the vector multicasted to the server group.
        if (SP_multicast(Mbox, AGREED_MESS, "Servers", 3, sizeof(struct vector), (const char *) Vector) < 0) {
                printf
        }
}*/
void recv_text(struct chat_packet *c){
/*Received text message*/
        struct chatrooms *r;
        printf("Received text: %s\n", c->text);
        lsequence++;
        c->sequence = lsequence*10 + atoi(server); /*Lamport timestamp */
        Last_packets[atoi(server)]->next = malloc(sizeof(struct node));
        Last_packets[atoi(server)]->next->data = malloc(sizeof(struct chat_packet));
        memcpy(Last_packets[atoi(server)]->next->data, c, sizeof(struct chat_packet));
        r = chatroomhead;
        while (r->next != NULL && (strncmp(r->name, c->group, strlen(c->group)) != 0))
        {
          r = r->next;
        }
	r->tail->next = malloc(sizeof(struct node));
	r->tail->next->data = malloc(sizeof(struct chat_packet));
	memcpy(r->tail->next->data, c, sizeof(struct chat_packet));
	r->tail = r->tail->next;

        write_data();
        Last_packets[atoi(server)] = Last_packets[atoi(server)]->next; /* Move pointer */
        strcat(c->group, server); /*Append server id for server's group */
        printf("Sending text to group %s\n", c->group);
        /* Send the message to the group */
        SP_multicast(Mbox, AGREED_MESS, c->group, 3, sizeof(struct chat_packet), (const char *) c);

}

void recv_join_msg(struct chat_packet *c) {
  int ret;
	/* Add server index to groupname */
        strcat(c->group, server);
        ret = SP_multicast(Mbox, AGREED_MESS, c->client_group, 3, sizeof(struct chat_packet), (const char *) c);
        /*send acknowledgement to the client's private group */
        if (ret < 0)
        {
                SP_error( ret );
        }
        /* Send room history to client */
        struct chat_packet *u;
        u = malloc(sizeof(struct chat_packet));
        struct chatrooms *r;
        struct node *i;
        r = chatroomhead;
        while (r->next != NULL && (strncmp(r->name, c->group, strlen(c->group)) != 0))
        {
          r = r->next;
        }
        if (strncmp(r->name, c->group, strlen(c->group)) != 0) /*Chat room doesn't exist*/
        {
          r->next = malloc(sizeof(struct chatrooms));
          r = r->next;
          r->head = malloc(sizeof(struct node));
          r->tail = r->head;
          strncpy(r->name, c->group, strlen(c->group)); /*Copy name to chatroom */
        }
        i = r->head;
        while (i->next != NULL)
        {
          if (strncmp(i->data->group, c->group, strlen(c->group)) == 0)
          {
            memcpy(u, i->data, sizeof(struct chat_packet));
            ret = SP_multicast(Mbox, AGREED_MESS, c->client_group, 3, sizeof(struct chat_packet), (const char *) c);
            i = i->next;
          }
        }


}

void recv_client_msg(struct chat_packet *c) {
   int ret;
   printf("Got packet type %d\n", c->type);
   if (c->type == 0 || c->type == 1)
   {
	recv_text(c);
   }
   else if (c->type == 2)
   {
	c->server_id = atoi(server); /*Set server id of packet */
	ret = SP_multicast(Mbox, AGREED_MESS, c->text, 3, sizeof(struct chat_packet), (const char *) c); 
	/*send acknowledgement to the client's private group */
	if (ret < 0)
	{
		SP_error( ret );
	}
   }
   else if (c->type == 5) /* Request to join group */
   {
	recv_join_msg(c);

   }
}

void recv_update() {

}

/*
 * Currently write_data only writes the packets (not the nodes) to disk.
 * This is because writing the entire nodes is meaningless because the
 * "next" and "data" pointers will become useless (?).
 */
void write_data() {
  if (Last_written->next == NULL)
  {
	
  }
  while (Last_written->next != NULL) {
    Last_written = Last_written->next;
    fwrite(Last_written->data, sizeof(struct chat_packet), 1, fp);
    fflush(fp);
  }
}

void memb_change() {

}

static void Bye() {
	To_exit = 1;
	printf("\nBye.\n");
	SP_disconnect( Mbox );
	exit( 0 );
}

static void Read_message() 
{
static  char             mess[MAX_MESSLEN];
        char             sender[MAX_GROUP_NAME];
        char             target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
        membership_info  memb_info;
        vs_set_info      vssets[MAX_VSSETS];
        unsigned int     my_vsset_index;
        int              num_vs_sets;
        char             members[MAX_MEMBERS][MAX_GROUP_NAME];
        int              num_groups;
        int              service_type;
        int16            mess_type;
        int              endian_mismatch;
        int              i,j;
        int              ret;
        service_type = 0;

        ret = SP_receive( Mbox, &service_type, sender, 100, &num_groups, target_groups, 
                &mess_type, &endian_mismatch, sizeof(mess), mess );
        printf("\n============================\n");
        if( ret < 0 ) 
        {
                if ( (ret == GROUPS_TOO_SHORT) || (ret == BUFFER_TOO_SHORT) ) {
                        service_type = DROP_RECV;
                        printf("\n========Buffers or Groups too Short=======\n");
                        ret = SP_receive( Mbox, &service_type, sender, MAX_MEMBERS, &num_groups, target_groups, 
                                          &mess_type, &endian_mismatch, sizeof(mess), mess );
                }
        }
if (ret < 0 )
        {
                if( ! To_exit )
                {
                        SP_error( ret );
                        printf("\n============================\n");
                        printf("\nBye.\n");
                }
                exit( 0 );
        }
        if( Is_regular_mess( service_type ) )
        {
                mess[ret] = 0;
                if     ( Is_unreliable_mess( service_type ) ) printf("received UNRELIABLE ");
                else if( Is_reliable_mess(   service_type ) ) printf("received RELIABLE ");
                else if( Is_fifo_mess(       service_type ) ) printf("received FIFO ");
                else if( Is_causal_mess(     service_type ) ) printf("received CAUSAL ");
                else if( Is_agreed_mess(     service_type ) ) printf("received AGREED ");
                else if( Is_safe_mess(       service_type ) ) printf("received SAFE ");
                printf("message from %s, of type %d, (endian %d) to %d groups \n(%d bytes): %s\n",
                        sender, mess_type, endian_mismatch, num_groups, ret, mess );
		if (num_groups == 1) {
			if (strncmp(target_groups[0], server, 1)==0) {
			recv_client_msg((struct chat_packet *) mess);		
			}
			else { printf("First group: %s", target_groups[0]);
			}
		}
        }else if( Is_membership_mess( service_type ) )
        {
                ret = SP_get_memb_info( mess, service_type, &memb_info );
                if (ret < 0) {
                        printf("BUG: membership message does not have valid body\n");
                        SP_error( ret );
                        exit( 1 );
                }
if     ( Is_reg_memb_mess( service_type ) )
                {
                        printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n",
                                sender, num_groups, mess_type );
                        for( i=0; i < num_groups; i++ )
                                printf("\t%s\n", &target_groups[i][0] );
                        printf("grp id is %d %d %d\n",memb_info.gid.id[0], memb_info.gid.id[1], memb_info.gid.id[2] );

                        if( Is_caused_join_mess( service_type ) )
                        {
                                printf("Due to the JOIN of %s\n", memb_info.changed_member );
                                // Deal with join of new member by joining the member's
                                // private 
                        }else if( Is_caused_leave_mess( service_type ) ){
                                printf("Due to the LEAVE of %s\n", memb_info.changed_member );
                        }else if( Is_caused_disconnect_mess( service_type ) ){
                                printf("Due to the DISCONNECT of %s\n", memb_info.changed_member );
                        }else if( Is_caused_network_mess( service_type ) ){
                                printf("Due to NETWORK change with %u VS sets\n", memb_info.num_vs_sets);
                                num_vs_sets = SP_get_vs_sets_info( mess, &vssets[0], MAX_VSSETS, &my_vsset_index );
                                if (num_vs_sets < 0) {
                                        printf("BUG: membership message has more then %d vs sets. Recompile with larger MAX_VSSETS\n", MAX_VSSETS);
                                        SP_error( num_vs_sets );
                                        exit( 1 );
                                }
                                for( i = 0; i < num_vs_sets; i++ )
                                {
                                        printf("%s VS set %d has %u members:\n",
                                               (i  == my_vsset_index) ?
                                               ("LOCAL") : ("OTHER"), i, vssets[i].num_members );
                                        ret = SP_get_vs_set_members(mess, &vssets[i], members, MAX_MEMBERS);
                                        if (ret < 0) {
                                                printf("VS Set has more then %d members. Recompile with larger MAX_MEMBERS\n", MAX_MEMBERS);
                                                SP_error( ret );
                                                exit( 1 );
                                        }
                                        for( j = 0; j < (int) vssets[i].num_members; j++ )
                                                printf("\t%s\n", members[j] );
                                }
                        }
		 }else if( Is_transition_mess(   service_type ) ) {
                        printf("received TRANSITIONAL membership for group %s\n", sender );
                }else if( Is_caused_leave_mess( service_type ) ){
                        printf("received membership message that left group %s\n", sender );
                }else printf("received incorrecty membership message of type 0x%x\n", service_type );
        } else if ( Is_reject_mess( service_type ) )
        {
                printf("REJECTED message from %s, of servicetype 0x%x messtype %d, (endian %d) to %d groups \n(%d bytes): %s\n",
                        sender, service_type, mess_type, endian_mismatch, num_groups, ret, mess );
        }else printf("received message of unknown message type 0x%x with ret %d\n", service_type, ret);


        printf("\n");
        fflush(stdout);

}

void main(int argc, char **argv) 
{
  int i;
  int ret;
  char group[80];
  char messages[5];
  struct node first_node; // Sentinel node used for read_disk()
  chatroomhead = malloc(sizeof(struct chatrooms));
  sp_time test_timeout;
  test_timeout.sec = 5;
  test_timeout.usec = 0;
  strncpy(Spread_name, "10080", 5);
  if (argc != 2)
  { 
    printf("Usage: chat_server <server id (1-5)>\n");
    exit (0);
  }
  strncpy(server, argv[1], 1);
  printf("Chat Server %u running\n", server);
  E_init();
  ret = SP_connect_timeout( Spread_name, User, 0, 1, &Mbox, Private_group, test_timeout );
  if( ret != ACCEPT_SESSION ) {
    SP_error( ret );
    Bye();
  }
  printf("Server connected to %s with private group %s\n", Spread_name, Private_group );
  ret = SP_join(Mbox, argv[1]); 
  printf("Join group %d return:%d\n", server, ret);
  if (ret != 0) SP_error( ret );
  E_attach_fd( Mbox, READ_FD, Read_message, 0, NULL, HIGH_PRIORITY );
  ret = SP_join(Mbox, "Servers"); 
  printf("Join group %d return:%d\n", server, ret);
  if (ret != 0) SP_error( ret );

  // For clearing the data structure before use.
  for (i = 0; i < 5; i++) {
    Server_packets[i].data = NULL;
    Server_packets[i].next = NULL;
    Last_packets[i] = &Server_packets[i];
  }
  // Last_written starts out as a sentinel node.
  Last_written = &first_node;
  Last_written->data = NULL;
  Last_written->next = NULL;

  /*
   * File setup. Note that the files will just be named after the server
   * to which the file belongs. First tries to open an existing file to
   * read from and write to. If file does not exist, creates a new file for
   * writing
   *
   * Future updates: create a more useful file name.
   */
  if ((fp = fopen(argv[1], "r+")) == NULL) {
    fp = fopen(argv[1], "w");
  } else {
    // If the file already exists, read to recover data lost from a crash.
    read_disk();
  }

  for (;;)
  {
   E_handle_events();
  }

}

