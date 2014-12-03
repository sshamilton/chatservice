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
        printf(">");
	fflush(stdout);
}

void send_msg(char *mtext)
{
int ret;
struct chat_packet *c;
char group[80];
c = malloc(sizeof(struct chat_packet));
	if (strlen(chatroom) > 1 && connected > 0 && strlen(username) >0) 
	{
	 printf("Sending %s to group %s\n", mtext, chatroom);	
		c->type = 0;//Chat message type
		strncpy(c->text, mtext, strlen(mtext));
  		c->server_id = connected;
		sprintf(group, "%d", connected);
		strncpy(c->group, chatroom, strlen(chatroom));
		strncpy(c->name, username, strlen(username));
		c->sequence = 0; //Server updates this
		c->resend = 0; //Not sure we need this anymore
		/* Send Message */
		ret = SP_multicast(Mbox, AGREED_MESS, group, 2, sizeof(struct chat_packet), (char *)c);
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
void like_msg(int like)
{
	int ret;
	struct chat_packet *c = malloc(sizeof(struct chat_packet));
	struct node *i = chatroom_start->next;
        while (i->sequence != like) (i=i->next);
          if (strlen(chatroom) > 1)
          {
                c->type = 1;//Chat like type
                c->server_id = i->data->server_id;
                strncpy(c->name, username, strlen(username));
                strncpy(c->group, chatroom, strlen(chatroom));
                c->sequence = i->data->sequence; 
                /* Send Message */
                ret = SP_multicast(Mbox, AGREED_MESS, chatroom, 2, sizeof(struct chat_packet), (char *) c);
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
        /* Send Message */
        ret = SP_multicast(Mbox, AGREED_MESS, server_id, 2, sizeof(struct chat_packet), (char *)c);
        if( ret < 0 )
        {
                SP_error( ret );
        }
}

void join_room(char *group)
{
  int ret;
  if ( group == "1" || group == "2" || group == "3" || group == "4" || group == "5")
  {
    printf("Chatrooms cannot be 1-5.  These are reserved\n>");
  }
  else if (strlen(username) < 1)
  {
    printf("Please set your username before joining a chat room\n>");
  }
  else {
    ret = SP_join( Mbox, group );
    if( ret < 0 ) SP_error( ret );
    strncpy(chatroom, group, strlen(group));
    chatroom_start = malloc(sizeof(struct node));
    chatroom_latest = chatroom_start;
  }

}

void print_history()
{
  struct node *t;
  t = chatroom_start; /*Head pointer is assumed to be null */
  int likes = 0;
  struct likes *i;
  if (chatroom != NULL)
  {
    while (t->next != NULL)
    {
      i = t->next->data->likes.next;
      while (i != NULL) {
	likes++;
	i= i->next;
      }
      printf("%d: %s\t(%u likes)\n", t->sequence, t->next->data->text, likes);
      t = t->next;
      likes = 0;
    }
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
        c = malloc(sizeof(struct chat_packet));
	c->type = 1;//Chat like type
        c->server_id = connected;
        strncpy(c->name, username, strlen(username));
        strncpy(c->group, chatroom, strlen(chatroom));
        c->sequence = 0; //Server updates this
        c->resend = 0; //Not sure we need this anymore
        /* Send Message */
        ret = SP_multicast(Mbox, AGREED_MESS, chatroom, 2, sizeof(struct chat_packet), (char *) c);
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

	for( i=0; i < sizeof(command); i++ ) command[i] = 0;
	if( fgets( command, 130, stdin ) == NULL ) 
            Bye();

	switch( command[0] )
	{
		case 'j':
			ret = sscanf( &command[2], "%s", group );
			if( ret < 1 ) 
			{
				printf(" invalid chatroom \n>");
				break;
			}
			join_room(group);
			break;
                case 'c':
			ret = sscanf( &command[2], "%s", group );
                        if( ret < 1 )
                        {
                                printf(" invalid server id \n>");
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
				printf(" Invalid Username\n>");
				break;
			}
			else
			{
				printf("Username set to %s\n>", username);
			}
			break;
 		case 'a':
			strncpy(mtext, &command[2], 140);
			if( strlen(mtext) < 1)
			{
				printf(" invalid message\n>");
				break;
			}
			send_msg(mtext);
			break;
		case 'l':
			ret = sscanf( &command[2], "%d", &like  );
                        if( ret < 1)
                        {
                                printf(" invalid line\n>");
                                break;
                        }
                        like_msg(like);
			break;
		case 'h':
			print_history();
			break;
		case 'v':
			show_servers();
			break;
		default: 
			printf(">");
			break;
    }
}
void recv_server_msg(struct chat_packet *c) {
   int ret;
   printf("Got packet type %d\n", c->type);
   if (c->type == 0) /*Message packet */
   {
	line_number++;
	chatroom_latest->next = malloc(sizeof(struct node));	
	chatroom_latest = chatroom_latest->next; /*Advance the pointer */
        chatroom_latest->data = malloc(sizeof(struct chat_packet));
        chatroom_latest->sequence = line_number;
	memcpy(chatroom_latest->data, c, sizeof(struct chat_packet));
	printf("%d:%s> %s",line_number, c->name, c->text);
   }
   else if (c->type == 1) /*like message */
   {
       struct node *i;
       struct likes *l;
       printf("Got like message from user %s, for message %d, Text:%s", c->name, c->sequence, c->text);
       i = chatroom_start->next; /* Setup iterator */
       printf("Istate: seq %u id: %u", i->data->sequence, i->data->server_id);
       while ((i !=NULL) && ((i->data->sequence != c->sequence) || (i->data->server_id != c->server_id)))
       {
	 i = i->next; printf("LOOP\n");
       }
	 if (i != NULL)
	 { printf("Data Seq: %u, serverid: %u\n", c->sequence, c->server_id);
	   l = &(i->data->likes);
           while (l->next != NULL) l = l->next;
	   l->next = malloc(sizeof(struct likes));
           strncpy(l->next->name, c->name, 20);  
	   printf("Created like for msg:%s, seq:%u", i->data->text, i->data->sequence); 
         }

   }
   else if (c->type == 2)
   {
	printf("Successful connection to server %d", c->server_id);
	connected = c->server_id;
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
                /*printf("message from %s, of type %d, (endian %d) to %d groups \n(%d bytes): %s\n",
                        sender, mess_type, endian_mismatch, num_groups, ret, mess ); */
		recv_server_msg((struct chat_packet *) mess);
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
        printf("> ");
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

  //ret = SP_join(Mbox, group); printf("Join group %s:%d\n", group, ret);
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

