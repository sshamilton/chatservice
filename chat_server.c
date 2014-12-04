#include "net_include.h"
/* global variables */
static  char    User[MAX_PRIVATE_NAME];
static  char    Spread_name[80];
static  char    Private_group[MAX_GROUP_NAME];
static  mailbox Mbox;
static  int vector[6][6]; 
static  int     Num_sent;
static  unsigned int    Previous_len;
static  int     To_exit = 0;
static  struct node     Server_packets[5];
// Essentially the 5 tail nodes of the 5 linked lists.
static  struct node*    Last_packets[5];
static  struct node*    Last_written[5];
static  char	server[1];
static  FILE   *fp;
static  int    lsequence = 0;
static struct chatrooms* chatroomhead;
static  int    servers_available = 0;
static  int	vectors_received = 0;

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
  struct chat_packet *c_temp;
  c_temp = malloc(sizeof(struct chat_packet));
  struct chatrooms *rooms;
  struct node *temp, *temp2;
  rooms = chatroomhead;
  while (fread(c_temp, sizeof(struct chat_packet), 1, fp) != 0) {
    i = c_temp->server_id - 1;

    // Allocate memory for the next node.
    Last_packets[i]->next = (struct node *) malloc(sizeof(struct node));

    // Update the pointer to new tail node.
    Last_packets[i] = Last_packets[i]->next;
    Last_written[i] = Last_written[i]->next;

    // Allocate memory for the new node's data.
    Last_packets[i]->data = (struct chat_packet *) malloc(sizeof(struct chat_packet));

    // memcpy the read data into the newly allocated memory for the next node.
    memcpy(Last_packets[i]->data, c_temp, sizeof(struct chat_packet));

    while (rooms->next != NULL)
    {
       rooms = rooms->next;
       if (strncmp(rooms->name, c_temp->group, strlen(c_temp->group)) == 0)
	{
	  /*chatroom exists*/
	  foundroom = 1;
	  break;
	}
    }
    if (!foundroom) /*room doesn't exist, so create it */
    {
       rooms->next = malloc(sizeof(struct chatrooms));
       rooms = rooms->next;
       rooms->head = malloc(sizeof(struct node));
       strncpy(rooms->name, c_temp->group, strlen(c_temp->group));
       rooms->tail = rooms->head;
    }

    temp = rooms->head;
    while (temp->next != NULL) {
       if (c_temp->sequence < temp->next->data->sequence) {
	 temp2 = temp->next;
	 temp->next = malloc(sizeof(struct node));
	 temp->next->data = malloc(sizeof(struct chat_packet));
	 memcpy(temp->next->data, c_temp, sizeof(struct chat_packet));
	 temp->next->next = temp2;
	 break;
       }
       temp = temp->next;
    }

    if (temp->next == NULL) {
	rooms->tail->next = malloc(sizeof(struct node));
	rooms->tail = rooms->tail->next;
	rooms->tail->data = malloc(sizeof(struct chat_packet));
	memcpy(rooms->tail->data, c_temp, sizeof(struct chat_packet));
    }

    // Clear the new node's next pointer for safety.
    Last_packets[i]->next = NULL;

    rooms = chatroomhead;
  }
    /*Update our LTS */
    lsequence = (Last_packets[atoi(server)]->sequence - atoi(server))/10;
    printf("Setting our LTS sequence to %d\n", lsequence);
    /*For debugging list chatrooms, and chats  loaded*/
    printf("Loaded from disk:\n");
    rooms = chatroomhead->next;
    while(rooms != NULL)
    {
	printf("Room: %s\n", rooms->name);
	struct node* i;
	i = rooms->head->next;
	while (i != NULL)
	{
	  printf("|%u|", i->data->sequence);
	  i = i->next;
	}
	printf("\n");
	rooms = rooms->next;
	
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
	/* Send message to servers */
	c->group[strlen(c->group) -1] = '\0'; /*Remove the trailing server id*/
	SP_multicast(Mbox, AGREED_MESS, "Servers", 3, sizeof(struct chat_packet), (const char *) c);

}

void recv_server_msg(struct chat_packet *c){
	struct chatrooms *r;
        printf("Received text: %s\n", c->text);
        lsequence++;
        Last_packets[c->server_id]->next = malloc(sizeof(struct node));
        Last_packets[c->server_id]->next->data = malloc(sizeof(struct chat_packet));
        memcpy(Last_packets[c->server_id]->next->data, c, sizeof(struct chat_packet));
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
        r->tail->next = malloc(sizeof(struct node));
        r->tail->next->data = malloc(sizeof(struct chat_packet));
        memcpy(r->tail->next->data, c, sizeof(struct chat_packet));
        r->tail = r->tail->next;

        write_data();
        Last_packets[c->server_id] = Last_packets[c->server_id]->next; /* Move pointer */
        strcat(c->group, server); /*Append server id for server's group */
        printf("Sending text to group %s\n", c->group);
        /* Send the message to the group */
        SP_multicast(Mbox, AGREED_MESS, c->group, 3, sizeof(struct chat_packet), (const char *) c);
	/*Update vector */
	vector[atoi(server)][c->server_id] = c->sequence;

}

void recv_join_msg(struct chat_packet *c) {
  int ret;
  char groupname[MAX_GROUP_NAME];
	/* Add server index to groupname */
	strncpy(groupname, c->group, strlen(c->group));
        strcat(c->group, server);
        ret = SP_multicast(Mbox, AGREED_MESS, c->client_group, 3, sizeof(struct chat_packet), (const char *) c);
        /*send acknowledgement to the client's private group */
        if (ret < 0)
        {
                SP_error( ret );
        }
        /* Send room history to client */
        struct chat_packet *u;
        struct chatrooms *r;
        struct node *i;
        r = chatroomhead;
        while (r->next != NULL && (strncmp(r->name, groupname, strlen(c->group)) != 0))
        {
          r = r->next;
        }
        if (strncmp(r->name, groupname, strlen(groupname)) != 0) /*Chat room doesn't exist*/
        {
          r->next = malloc(sizeof(struct chatrooms));
          r = r->next;
          r->head = malloc(sizeof(struct node));
          r->tail = r->head;
          strncpy(r->name, groupname, strlen(groupname)); /*Copy name to chatroom */
        }
        i = r->head->next;
        while (i != NULL)
        {
          ret = SP_multicast(Mbox, AGREED_MESS, c->client_group, 3, sizeof(struct chat_packet), (const char *) i->data);
          i = i->next;
          
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

void send_all_after(int max, int c)
{
 /*Send all after LTS here */
  printf("Send lts %d", max);
}

void recv_update(int rvector[][6]) {
int max, c;
printf("Recevied vector\n");
int i, j;
for (i=1; i<6; i++)
{
  for (j=1; j<6; j++)
  {
   printf("|%d|", rvector[i][j]);
   if (rvector[i][j] > vector[i][j]) vector[i][j] = rvector[i][j];
  }
 printf("\n");
}

/*Determine if we have recevied all the updates */
vectors_received++;

if (vectors_received == servers_available -1) /*We got all the updates */
{
   for (i = 1; i<6; i++)
   {
     max=0;
     for (j=1; j<6; j++)
     {
	if (vector[j][i] > max)
	{
	   max = vector[j][i];
	   c = j;
	}
     }
     if (c == atoi(server)) /*We have the latest, so send all after LTS */
     {
	send_all_after(max, c);
     }
   }
}

}

/*
 * Currently write_data only writes the packets (not the nodes) to disk.
 * This is because writing the entire nodes is meaningless because the
 * "next" and "data" pointers will become useless (?).
 */
void write_data() {
	int i;
	for (i = 0; i < 5; i++) {
	  while (Last_written[i]->next != NULL) {
		Last_written[i] = Last_written[i]->next;
		fwrite(Last_written[i]->data, sizeof(struct chat_packet), 1, fp);
		fflush(fp);
	  }
	}
}

void memb_change() {
   vector[atoi(server)][atoi(server)] = lsequence *10 + atoi(server);
   /*Send vector*/
   SP_multicast(Mbox, AGREED_MESS, "Servers", 10, sizeof(int)*36, (const char *)vector);
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
			else if (strncmp(target_groups[0], "Servers", 7)==0 && (sender[1] != server[0]))
			{
			  printf("Recevied message from another server cmp %d %d\n", sender[1], server[0]);
			  if (mess_type == 10) /*Vector update */
			  {
				recv_update((int(*) [6]) mess);
			  }
			  else
			  {
			  recv_server_msg((struct chat_packet *) mess);
			  }
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
                            if (strncmp(sender, "Servers", 7)==0)
			    {
				servers_available = num_groups;  /*To determine how many servers to update */
				printf("Running membership change\n");
				memb_change();
			    }

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
  int i, j;
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
  ret = SP_connect_timeout( Spread_name, server, 0, 1, &Mbox, Private_group, test_timeout );
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
    Last_written[i] = &Server_packets[i];
  }

  /* Initialize vector */
for (i=1; i<7; i++)
{  
  for (j=1; j<6; j++)
  {   
   vector[i][j] =0;
  }
 printf("\n");
}


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

