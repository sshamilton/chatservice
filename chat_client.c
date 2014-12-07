#include "net_include.h"

/* Global variables */
static  char    User[80];
static  char    Spread_name[80];
static  char    Private_group[MAX_GROUP_NAME];
static  mailbox Mbox;
static  int     Num_sent;
static  unsigned int    Previous_len;
static  int     To_exit = 0;
static  void    Bye();
static  int     connected = 0;
static  char    username[20];
static  char    chatroom[MAX_GROUP_NAME] ="\0";
static  int     line_number=0;
static  struct  node* chatroom_start;
static  struct  node* chatroom_latest;
static  void    Read_message();

void show_menu()
{
	printf("\n");
	printf("==========\n");
	printf("User Menu:\n");
	printf("----------\n");
	printf("\n");
	printf("\tc <server> -- Join Server\n");
	printf("\tu <name> -- Set Username\n");
	printf("\n");
	printf("\tj <room> -- join chatroom\n");
        printf("\ta <text> -- append line/send message\n");
	printf("\tl <line number> -- like message\n");
        printf("\tr <line number> -- remove like from message\n");
	printf("\th Print History\n");
	printf("\tv View Chat Servers Available\n");
	printf("\t? Display this menu\n");
	printf("\n");
	printf("\tq -- quit\n");
        printf("> ");
	fflush(stdout);
}

void send_msg(char *mtext)
{
int ret;
struct chat_packet *c;
char group[80];
c = malloc(sizeof(struct chat_packet));
char server_group[1];
sprintf(server_group, "%d", connected); /*Convert server id to string for spread groupname*/
	if (strlen(chatroom) > 1 && connected > 0 && strlen(username) >0) 
	{
	 //printf("Sending %s to group %s\n", mtext, chatroom);	
		c->type = 0;//Chat message type
		strncpy(c->text, mtext, strlen(mtext));
  		c->server_id = connected;
		strncpy(c->group, chatroom, strlen(chatroom));
		strncpy(c->name, username, strlen(username));
		c->sequence = 0; //Server updates this
		//c->resend = 0; //Not sure we need this anymore
		/* Send Message */
		//printf("Sending to group %s\n", server_group);
		ret = SP_multicast(Mbox, AGREED_MESS, server_group, 2, sizeof(struct chat_packet), (char *)c);
		if( ret < 0 )
                {
                	SP_error( ret );
                }
	}
	else 
	{
		if (strlen(chatroom) <= 1) {
		printf("Sorry, you must join a chat room to send a message.\n");
		}
		else if (strlen(username) < 1) {
		printf("Please set your username before sending a message.\n");
 		}

		else {
		printf("Please connect to a server before trying to send a message.\n");
		}
	}
}
void like_msg(int like, int ul)
{
	int ret;
	struct chat_packet *c = malloc(sizeof(struct chat_packet));
	struct node *i = chatroom_start->next;
	char server_group[1];
	if (like > line_number) {
		printf("Nonexistent line\n> ");
		return;
	}
        sprintf(server_group, "%d", connected); /*Convert server id to string for spread groupname*/
        while (i->sequence != like) (i=i->next);
          if (strlen(chatroom) > 1)
          {
		if (ul == 1) {
                  c->type = 1;//Chat like type
		} else c->type = 7; // Unlike
                c->server_id = connected;
                strncpy(c->name, username, strlen(username));
                strncpy(c->group, chatroom, strlen(chatroom));
                c->lts = i->data->sequence; 
                /* Send Message */
                ret = SP_multicast(Mbox, AGREED_MESS, server_group, 2, sizeof(struct chat_packet), (char *) c);
                if( ret < 0 )
                {
                        SP_error( ret );
                }
          }
          else
          {
                printf("Sorry, you must join a chat room to like a message.\n");
          }
       
}

void join_server(char *server_id)
{
	int ret;
	struct chat_packet *c;
	struct chat_packet *r;
	char             mess[MAX_MESSLEN];
        char             sender[MAX_GROUP_NAME];
        char             target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
        membership_info  memb_info;
        vs_set_info      vssets[MAX_VSSETS];
        unsigned int     my_vsset_index;
        int              num_vs_sets;
        char             members[MAX_MEMBERS][MAX_GROUP_NAME];
        int              num_groups;
        int              service_type=0;
        int16            mess_type;
        int              endian_mismatch;
	char		group[80];
	c = malloc(sizeof(struct chat_packet));
	r = malloc(sizeof(struct chat_packet));
	c->type = 2;//Request to join server
        strncpy(c->text, Private_group, strlen(Private_group));
        c->server_id = atoi(server_id);
        //printf("Sending private grup %s to server", Private_group);
	strncpy(c->client_group, Private_group, MAX_GROUP_NAME);
        /* Send Message */
	if (connected > 0)
	{
	  /*Leave current group*/
	  sprintf(group, "c%d", connected);
	  SP_leave(Mbox, group);
	}
        /* ret = SP_multicast(Mbox, AGREED_MESS, server_id, 2, sizeof(struct chat_packet), (char *)c); */
	/* That was the old way.  New way is join the server group */
	sprintf(group, "c%d", atoi(server_id));
	ret = SP_join(Mbox, group);
	connected = atoi(server_id); /*Set connected.  Note we still need response from server, but we need to pass this to the recv connect */
        if( ret < 0 )
        {
                SP_error( ret );
        }
}

void join_room(char *group)
{
  int ret;
  struct chat_packet *c;
  char server_group[1];
  sprintf(server_group, "%d", connected); /*Convert server id to string for spread groupname*/ 
  c = malloc(sizeof(struct chat_packet));
  if ( group == "1" || group == "2" || group == "3" || group == "4" || group == "5")
  {
    printf("Chatrooms cannot be 1-5.  These are reserved\n>");
  }
  else if (strlen(username) < 1)
  {
    printf("Please set your username before joining a chat room\n>");
  }
  else {
  /*Disjoin from current group */
  if (strlen(chatroom) > 0)
  {
	sprintf(chatroom, "%s%d",chatroom, connected);
	printf("leaving chatroom, %s\n", chatroom);
	SP_leave(Mbox, chatroom);
  }
  //printf("chatroom length = %d\n", strlen(chatroom));
  /* Send request to server to join a group */
  strncpy(c->client_group, Private_group, MAX_GROUP_NAME);
  strncpy(c->name, username, strlen(username));
  //printf("Sending join with %s", Private_group);
  strncpy(c->group, group, strlen(group));
  c->type = 5;
  ret = SP_multicast(Mbox, AGREED_MESS, server_group, 2, sizeof(struct chat_packet), (char *)c);
        if( ret < 0 )
        {
                SP_error( ret );
        }
  }

}

void print_history()
{
  struct node *t;
  t = chatroom_start; /*Head pointer is assumed to be null */
  if (chatroom != NULL)
  {
    while (t->next != NULL)
    {
    printf("%d: %s>%s (%u likes)\n", t->next->sequence, t->next->data->name, t->next->data->text, t->next->data->num_likes);
      t = t->next;
	    }
	  } 
  else 
  {
    printf("You must be in a chatroom to view history!\n>");
  }
}

void print_after(int lts)
{
  struct node *t;
  t = chatroom_start; /*Head pointer is assumed to be null */
  printf("\n");
  if (chatroom != NULL)
  {
    while (t->next != NULL)
    {
     if (t->next->data->sequence >= lts)
     {
      printf("%d: %s>%s (%u likes)\n", t->next->sequence, t->next->data->name, t->next->data->text, t->next->data->num_likes);
     }
    t = t->next;
    }
   printf("\n>");
  }
  else
  {
    printf("You must be in a chatroom to view history!\n>");
  }
}


void show_servers()
{
	int ret;
        struct chat_packet *c;
	char server_group[1];
        c = malloc(sizeof(struct chat_packet));
	c->type = 8;//Request online server list.
        c->server_id = connected;
        strncpy(c->name, username, strlen(username));
	strncpy(c->client_group, Private_group, MAX_GROUP_NAME);
	sprintf(server_group, "%d", connected); /*Convert server id to string for spread groupname*/
        c->sequence = 0; //Server updates this
        /* Send Message */
        ret = SP_multicast(Mbox, AGREED_MESS, server_group, 2, sizeof(struct chat_packet), (char *) c);
        if( ret < 0 )
        {
                SP_error( ret );
        }
}
static	void	User_command()
{
	char	command[130];
	char	mtext[80];
	char	mess[MAX_MESSLEN];
	char	group[80];
	char	groups[10][MAX_GROUP_NAME];
	int	num_groups;
	unsigned int	mess_len;
	int	ret;
	int	i;
	int 	like;
	sp_time test_timeout;
	char    server_id[1];

	for( i=0; i < sizeof(command); i++ ) command[i] = 0;
	if( fgets( command, 130, stdin ) == NULL ) 
            Bye();
	/* remove newline from string */
	command[strlen(command) -1] = 0;
	switch( command[0] )
	{
		case 'j':
			ret = sscanf( &command[2], "%s", group );
			if( ret < 1 ) 
			{
				printf(" invalid chatroom \n> ");
				break;
			}
			join_room(group);
			break;
                case 'c':
			ret = sscanf( &command[2], "%s", group );
                        if( ret < 1 )
                        {
                                printf(" invalid server id \n> ");
                                break;
                        }
		  	join_server(group);
                        break;
  		case 'q':
			Bye();
			exit(0);
			break;
  		case '?':
			show_menu();
			break;
   		case 'u':
			ret = sscanf( &command[2], "%s", username);
			if( ret < 1 )
			{
				printf(" Invalid Username\n> ");
				break;
			}
			else
			{
				printf("Username set to %s\n> ", username);
				/*If in chatroom, disconnect */
				if (strlen(chatroom) > 0)
  				{
        				sprintf(chatroom, "%s%d",chatroom, connected);
        				printf("leaving chatroom, %s due to username change\n", chatroom);
        				SP_leave(Mbox, chatroom);
  				} 
			}
			break;
 		case 'a':
			strncpy(mtext, &command[2], 130);
			if( strlen(mtext) < 1)
			{
				printf(" invalid message\n> ");
				break;
			}
			send_msg(mtext);
			break;
		case 'l':
			ret = sscanf( &command[2], "%d", &like  );
                        if( ret < 1)
                        {
                                printf(" invalid line\n> ");
                                break;
                        }
                        like_msg(like, 1);
			break;
		case 'r':
			ret = sscanf( &command[2], "%d", &like);
			if (ret < 1)
			{
				printf(" invalid line\n> ");
				break;
			}
			like_msg(like, 0);
			break;
		case 'h':
			print_history();
    			printf("> ");
			break;
		case 'v':
			show_servers();
			break;
		default: 
			printf("> ");
			break;
    }
   fflush(stdout); 
}
void recv_server_msg(struct chat_packet *c, int16 mess_type) {
   int ret;
   struct node *temp, *temp2;
   if (mess_type == 0) { // If the mess_type is 0, that means we already have the chat_packet, we just want to update the likes.
	temp = chatroom_start->next;
	while (temp != NULL) {
		if (temp->data->sequence == c->sequence) {
		  temp->data->num_likes = c->num_likes;
		  break;
		}
		temp = temp->next;
	}
	print_after(c->sequence);
   }
   else if (mess_type == 13) {
	temp = chatroom_start;
	int count= 0;
	/* We need to put the merged chats in the correct order */
	while (temp->next != NULL) {
	  temp->next->sequence = count;/* for resequencing*/
	  if (c->sequence < temp->next->data->sequence)
	  {
	     temp2 = temp->next;
             temp->next = malloc(sizeof(struct node));
             temp->next->data = malloc(sizeof(struct chat_packet));
             memcpy(temp->next->data, c, sizeof(struct chat_packet));
	     temp->next->sequence = count; 
	     temp->next->next = temp2;
	     break;
	  }
    	  temp = temp->next;
	}
        if (temp->next == NULL) /*We are at the end, so add the message */
	{
	   temp2 = temp->next;
           temp->next = malloc(sizeof(struct node));
           temp->next->data = malloc(sizeof(struct chat_packet));
           memcpy(temp->next->data, c, sizeof(struct chat_packet));
	   temp->next->sequence = count;
           temp->next->next = temp2;
	   chatroom_latest = temp->next; /*set latest to this new packet */ 
	}
	/*Renumber */
	temp = chatroom_start;
        while (temp->next !=NULL)
	{
 	  count++;
	  temp->next->sequence = count;
	  temp = temp->next;
	}
	count++;
	line_number = count;
	print_after(c->sequence); /*Print out where we added the packet */
   }
   else if (c->type == 0 || c->type == 3) /*Message packet */
   {
	if (c->type == 0) line_number++; /*Only update line number on text message */
	chatroom_latest->next = malloc(sizeof(struct node));	
	chatroom_latest = chatroom_latest->next; /*Advance the pointer */
        chatroom_latest->data = malloc(sizeof(struct chat_packet));
        chatroom_latest->sequence = line_number;
	memcpy(chatroom_latest->data, c, sizeof(struct chat_packet));
        if (c->type == 0) /* Live message, display it */
	{
	  printf("%d:%s> %s (%d likes)\n>",line_number, c->name, c->text, c->num_likes);
	}
   }
   else if (c->type == 2)
   {
	printf("Successful connection to server %d\n>", c->server_id);
	connected = c->server_id;
   }
   else if (c->type == 5) /* Received response to join group */
   {
     ret = SP_join( Mbox, c->group );
     if( ret < 0 ) SP_error( ret );
     bzero(chatroom, MAX_GROUP_NAME);
     strncpy(chatroom, c->group, strlen(c->group)-1); /*Remove server id from chatroom group name */
     chatroom_start = malloc(sizeof(struct node));
     chatroom_latest = chatroom_start;
     printf("Successfully joined group %s\n>", chatroom); /* don't display server index at end of group */
     line_number = 0; /*Refresh line number */
   }
   else if (c->type == 6) /*Refresh screen all after lamport timestamp */
   {
     struct node *i;
     i = chatroom_start->next; /* Setup iterator */
     line_number = 0;
     while ((i !=NULL) && ((i->data->sequence != c->sequence) || (i->data->server_id != c->server_id)))
     {  line_number++;
	i = i->next;
     }
     while (i != NULL)
    {
	line_number++;
	printf("%d:%s> %s",line_number, i->data->name, i->data->text);
    }
   }
   else if (c->type == 8) /*Display servers online */
   {
	printf("Servers that are online are: %s\n>", c->text);
   }
   else if (c->type == 9) /*Updated name list for chatroom */
   {
	printf("Members in chatroom are now: %s\n>", c->text);
   }

}
static  void    Read_message()
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
        int              ret, success =0;
	char		 server_group[1];
	char		 *uname;

        service_type = 0;

        ret = SP_receive( Mbox, &service_type, sender, 100, &num_groups, target_groups, 
                &mess_type, &endian_mismatch, sizeof(mess), mess );
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
                //printf("message from %s, of type %d, (endian %d) to %d groups \n(%d bytes): %s\n",
                //        sender, mess_type, endian_mismatch, num_groups, ret, mess ); 
	 	//printf("private group is %s\n", Private_group);
		if (strncmp(Private_group, sender, strlen(Private_group)) != 0)
		{	
		   recv_server_msg((struct chat_packet *) mess, mess_type);
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
                        //printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n",
                        //        sender, num_groups, mess_type );
                        //for( i=0; i < num_groups; i++ )
                        //        printf("\t%s\n", &target_groups[i][0] );
                        //printf("grp id is %d %d %d\n",memb_info.gid.id[0], memb_info.gid.id[1], memb_info.gid.id[2] );
			sprintf(server_group, "c%d", connected);
			if (strncmp(sender, server_group, 2) == 0)
			{
			  //printf("Server connection message\n");
			  for (i=0; i< num_groups; i++)
			  {
			    if (target_groups[i][1] == server_group[1])
			    {
				printf("Successfully connected to server %d\n>", connected);
				success = 1;
			    }
			  }
			  if (!success)
			  {
				printf("Server unavailable.  Please try again later.\n>");
				SP_leave(Mbox, sender);
			  }
			}
			else
			{ /*Membership message from chatroom.  Show users in chat */
			 /*No longer here*/ 
			}
                        if( Is_caused_join_mess( service_type ) )
                        {
                                //printf("Due to the JOIN of %s\n", memb_info.changed_member );
                        }else if( Is_caused_leave_mess( service_type ) ){
                                //printf("Due to the LEAVE of %s\n", memb_info.changed_member );
                        }else if( Is_caused_disconnect_mess( service_type ) ){
                                //printf("Due to the DISCONNECT of %s\n", memb_info.changed_member );
			        if (strncmp(sender, server_group, 2) == 0 && memb_info.changed_member[1] == server_group[1])
				{
				  printf("Disconnected from server %s\n", server_group);
				  SP_leave(Mbox, sender); /*Disconnect us from the server group */
				  if (chatroom != NULL)
				  {
				    sprintf(chatroom, "%s%d",chatroom, connected);
				    SP_leave(Mbox, chatroom);
				    chatroom[0]='\0';
				    chatroom_start = malloc(sizeof(struct node));
				  }
				  connected = 0; /*we are no longer connected */
				}
                        }else if( Is_caused_network_mess( service_type ) ){
                                //printf("Due to NETWORK change with %u VS sets\n", memb_info.num_vs_sets);
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
		}
                
		else if( Is_transition_mess( service_type ) ) {
                        //printf("received TRANSITIONAL membership for group %s\n", sender );
                }else if ( Is_caused_leave_mess( service_type ) ){
                        //printf("received membership message that left group %s\n", sender );
		} else printf("Incorrect mess type");
		
        }  else if (Is_reject_mess( service_type)) {
		printf("Rejcted");
	}  else printf("Unknown");
        fflush(stdout);
}

void main()
{
  int ret;
  E_init();
  sp_time test_timeout;
  test_timeout.sec = 5;
  test_timeout.usec = 0;
  strncpy(Spread_name, "10080", 5);
  ret = SP_connect_timeout( Spread_name, User, 0, 1, &Mbox, Private_group, test_timeout );
        if( ret != ACCEPT_SESSION ) 
        {
                SP_error( ret );
                Bye();
        }
        printf("Connected to %s with private group %s\n", Spread_name, Private_group );

  E_attach_fd( 0, READ_FD, User_command, 0, NULL, LOW_PRIORITY );
  E_attach_fd( Mbox, READ_FD, Read_message, 0, NULL, HIGH_PRIORITY );
  show_menu();
  for(;;)
  {
    E_handle_events();
    User_command();
  }
  exit (0);
}

static  void	Bye()
{
	To_exit = 1;
	printf("\nBye.\n");
	SP_disconnect( Mbox );
	exit( 0 );
}

