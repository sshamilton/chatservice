#include "net_include.h"
/* global variables */
static  char    User[MAX_PRIVATE_NAME];
static  char    Spread_name[80];
static  char    Private_group[MAX_GROUP_NAME];
static  mailbox Mbox;
static  int     Num_sent;
static  unsigned int    Previous_len;
static  int     To_exit = 0;
static  struct node     Server_packets[5];
static  FILE*   fd;
// Pointer to last node that has been written to disk
static  struct node*    last_written;



void read_disk();
void send_vector();
void recv_update();
void recv_client_msg();
void write_data();
void memb_change();


/*
 * Read disk is used for recovering from crashes. Things to do:
 * 1. Restore data structure and recover data for users connected to the
 * server.
 * 2. Restore the vector.
 */
void read_disk(FILE *fp) {
  int                 i = 0;
  struct chat_packet* c_temp;
  struct node*        n_temp[5];

  // Needs to be fixed!
  while (fread(c_temp, sizeof(struct chat_packet), 1, fp) != 0) {
    // Change to new array.
    if ((c_temp->server_id - 1) != i) {
      i = c_temp->server_id - 1;
      n_temp[i] = &Server_packets[i];
    }

    // Allocate memory for the next node.
    n_temp[i]->next = (struct node *) malloc(sizeof(struct node));
    // Update the n_temp pointer to new tail node.
    n_temp[i] = n_temp[i]->next;
    // Allocate memory for the new node's data.
    n_temp[i]->data = (struct chat_packet *) malloc(sizeof(struct chat_packet));
    // memcpy the read data into the newly allocated memory for the next node.
    memcpy(n_temp[i]->data, c_temp, sizeof(struct chat_packet));
    // Clear the new node's next pointer for safety.
    n_temp[i]->next = NULL;
  }
}
 
void send_vector() {

}

void recv_update() {

}

void recv_client_msg() {

}

/*
 * Currently write_data only writes the packets (not the nodes) to disk.
 * This is because writing the entire nodes is meaningless because the
 * "next" and "data" pointers will become useless (?).
 */
void write_data() {
  // Iterate starting from the most recently written to disk and then write from that
  // point on following the "next_seq" pointers

  while (last_written->next != NULL) {
    last_written = last_written->next;
    fwrite(last_written->data, sizeof(struct chat_packet), 1, fd);
  }

  // Not what we want I think:
  /* Iterate through each server linked list and write the data to fd. Not sure if this is
   exactly what we want -- maybe need some input parameters to tell us from what point
   to start writing to the disk.

  int i;
  struct node* temp;
  for (i = 0; i < 5; i++) {
    temp = Server_packets[i];
    while (temp.next != NULL) {
      temp = temp.next;
      fwrite(temp.data, sizeof(chat_packet), 1, fd);
    }
  }*/
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
        printf("User> ");
        fflush(stdout);

}

void main(int argc, char **argv) 
{
  int i;
  int server;
  int ret;
  char group[80];
  char messages[5];
  FILE *fp;
  sp_time test_timeout;
  test_timeout.sec = 5;
  test_timeout.usec = 0;
  strncpy(Spread_name, "10080", 5);
  if (argc != 2)
  { 
    printf("Usage: chat_server <server id (1-5)>\n");
    exit (0);
  }
  server = atoi(argv[1]);
  printf("Chat Server %u running\n", server);
  E_init();
  ret = SP_connect_timeout( Spread_name, User, 0, 1, &Mbox, Private_group, test_timeout );
  if( ret != ACCEPT_SESSION ) {
    SP_error( ret );
    Bye();
  }
  printf("Serer connected to %s with private group %s\n", Spread_name, Private_group );
  ret = SP_join(Mbox, argv[1]); 
  printf("Join group %d return:%d\n", server, ret);
  if (ret != 0) SP_error( ret );
  E_attach_fd( Mbox, READ_FD, Read_message, 0, NULL, HIGH_PRIORITY );

  // For clearing the data structure before use.
  for (i = 0; i < 5; i++) {
    Server_packets[i].data = NULL;
    Server_packets[i].next = NULL;
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
    read_disk(fp);
  }

  for (;;)
  {
   ret =0;
  }

}

