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
	printf("\th Print History\n");
	printf("\tv View Chat Servers Available\n");
	printf("\t? Display this menu\n");
	printf("\n");
	printf("\tq -- quit\n");
	fflush(stdout);
}

void send_msg();
void update_like();
void join_server();
void set_username();
void join_room();
void print_history();
void show_servers();
static	void	User_command()
{
	char	command[130];
	char	mess[MAX_MESSLEN];
	char	group[80];
	char	groups[10][MAX_GROUP_NAME];
	int	num_groups;
	unsigned int	mess_len;
	int	ret;
	int	i;

	for( i=0; i < sizeof(command); i++ ) command[i] = 0;
	if( fgets( command, 130, stdin ) == NULL ) 
            Bye();

	switch( command[0] )
	{
		case 'j':
			ret = sscanf( &command[2], "%s", group );
			if( ret < 1 ) 
			{
				printf(" invalid chatroom \n");
				break;
			}
			if ( group == "1" || group == "2" || group == "3" || group == "4" || group == "5")
 		        {
			     	printf("Chatrooms cannot be 1-5.  These are reserved\n");
			}
 		 	else {
				ret = SP_join( Mbox, group );
				if( ret < 0 ) SP_error( ret );
			}
			break;
                case 'c':
                        ret = sscanf( &command[2], "%s", group );
                        if( ret < 1 )
                        {
                                printf(" invalid server \n");
                                break;
                        }
                        ret = SP_join( Mbox, group );
                        if( ret < 0 ) { SP_error( ret );} else { connected=1;};

                        break;
    }
}
void main()
{
  E_init();
  //ret = SP_join(Mbox, group); printf("Join group %s:%d\n", group, ret);

  show_menu();
}

static  void	Bye()
{
	To_exit = 1;
	printf("\nBye.\n");
	SP_disconnect( Mbox );
	exit( 0 );
}

