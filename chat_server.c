#include "net_include.h"
/* global variables */
static  char			User[MAX_PRIVATE_NAME];
static  char			Spread_name[80];
static  char			Private_group[MAX_GROUP_NAME];
static  mailbox 		Mbox;
static  int				vector[6][6]; 
static  int     		Num_sent;
static  unsigned int	Previous_len;
static  int				To_exit = 0;
static  struct node		Server_packets[5];
// Essentially the 5 tail nodes of the 5 linked lists.
static  struct node		*Last_packets[5];
static  struct node		*Last_written[5];
static  char			server[1];
static  FILE			*fp;
static  int				lsequence = 0;
static	struct			chatrooms* chatroomhead;
static  int				servers_available = 0;
static  int				vectors_received = 0;
static  int				servers_online[6];
void read_disk();
void send_vector();
void recv_client_msg();
void write_data();
void memb_change();
void showmatrix(int rvector[][6]);
void send_allnames();
void send_namelist(char *group, struct names *names);
struct node * empty_chatroom_node();
// Insert into the array of linked lists.
void array_ll_insert(struct chat_packet *, int);
// Insert a text message into the chatroom data structure.
struct node * chatroom_insert_msg(struct chat_packet *);
struct node * find_desired_msg(int, struct chatrooms *);
struct likes * find_user_like(char *, struct likes *);
// Insert a like message into the chatroom data structure.
struct node * chatroom_process_like(struct chat_packet *);
struct chatrooms * create_room(char *, struct chatrooms *);
struct chatrooms * find_room(char *);
struct node * find_insert_slot(int, struct chatrooms *);

/* Message Types */
/* 0 - Display chat text */
/* 1 - Like message */
/* 2 - Connect to server */
/* 3 - Receive message (to memory, don't display) */
/* 4 - Display all lines in chatroom after given lamport timestamp */
/* 5 - Request to joing group */
/* 6 - Refresh client screen */
/* 7 - Unlike message */
/* 8 - Display servers online */
/* 9 - Send client user list */
/* 10 - Send username/private name combo from server to server */
/* 11 - Remove username/private name combo from group server to server */

/*
 * Read disk is used for recovering from crashes. Things to do:
 * 1. Restore data structure and recover data for users connected to the
 * server.
 * 2. Restore the vector.
 */

void add_user(char *name, char *pname, struct names *names, int server_id)
{
  struct  names *i;
  struct  pnames *p;
  int founduser = 0;
  int foundclient = 0;
  i = names;
  
  while (i->next != NULL)
  {
     if (strcmp(i->next->name, name) == 0)
     {
       printf("User exists. Look for pname\n");
       p = i->next->pnames;
       while (p->next != NULL)
       {
	 if (strcmp(p->next->pname, pname) ==0)
	 {
	   foundclient = 1;
  	   break;
	 }
	p = p->next;
       }
       founduser = 1;
       break;
     }
     i = i->next;
  }
  if (!founduser)
  {
     printf("Adding user to array\n");
     i->next = (struct names *)calloc(1,sizeof(struct names));
     strncpy(i->next->name, name, strlen(name));
     i->next->pnames = (struct pnames *)calloc(1,sizeof(struct pnames));
     i->next->pnames->next = (struct pnames *)calloc(1,sizeof(struct pnames));
     strncpy(i->next->pnames->next->pname, pname, strlen(pname));
     i->next->pnames->next->server_id = server_id;
  }
  else if (!foundclient)
  {
     printf("username exists, adding client name\n");
     p = i->next->pnames;
     while (p->next != NULL)
     {
       p = p->next;
     }
     p->next = (struct pnames *)calloc(1,sizeof(struct pnames));
     strncpy(p->next->pname, pname, strlen(pname));
	 p->next->server_id = server_id;
  }
  else
  {
     printf("User and client exists\n");
  }
}

print_membership()
{
  struct chatrooms *r;
  struct names *n;
  struct pnames *p;

  r = chatroomhead;
  while (r->next != NULL)
  {
    printf("Room: %s\n", r->next->name);
    n = r->next->names;
    while (n->next != NULL)
    {
      p = n->next->pnames;
      printf("name: %s", n->next->name);
      while (p->next !=NULL)
      {
        printf("Pname: %s", p->next->pname);
        p = p->next;
      }
      n = n->next;
      printf("\n");
    }
    r = r->next;
    printf("\n");
  }
}

void rm_user(char *pname, struct names *names)
{
  printf("Membership before: \n");
  print_membership();
  struct names *i, *t;
  struct pnames *p, *tempp;
  struct names *tempi;
  i = names;
  int c;
  int removed = 0;
  while (i->next != NULL)
    {
	p = i->next->pnames;
	c=0;
        while (p->next != NULL)
	{
	  c++;
	  if (strcmp(p->next->pname, pname) == 0)
	  {
	    printf("Found pname.  C is %d, removing pname %s\n", c, pname);
	      /*Remove just this name */
	      tempp = p->next; /* Used to free */
	      p->next = p->next->next;
	      break;
	  }
 	p = p->next;
	}
	if (i->next->pnames->next == NULL)
            {
              /* only name in room, so take entire username out */
              tempi = i->next; /* Used to free */
   	      printf("Removing name %s", i->next->name);
              i->next = i->next->next;
              break;
            }

        if (i->next != NULL) i = i->next;
    }
  printf("Membership After:\n");
  print_membership();
}

void read_disk() {
	int 				i, foundroom =0, max =0;
	struct chat_packet	c_temp;
	struct likes 		*l;
	struct chatrooms	*rooms, *r;
	struct node			*temp, *temp2, *temp3;
        char tempname[25];

	while (fread(&c_temp, sizeof(struct chat_packet), 1, fp) != 0) {
		i = c_temp.server_id - 1;
		// Insert the read message into the array of linked lists.
		array_ll_insert(&c_temp, i);
		printf("Loaded message: %s, room: %s\n", c_temp.text, c_temp.group);

		// Update the pointer of the most recently written to disk.
		Last_written[i] = Last_written[i]->next;
	
		// If sequence is higher, update our max.
		if (c_temp.sequence > max) max = c_temp.sequence;

		if (c_temp.type == 1) {
			chatroom_process_like(&c_temp);
		} else {
			chatroom_insert_msg(&c_temp);
		}

		/* Update our LTS vector */
		if (vector[atoi(server)][c_temp.server_id] < c_temp.sequence)
		{
			vector[atoi(server)][Last_packets[i]->data->server_id] = c_temp.sequence;
		}
	}

	printf("Vector after load:\n");
	showmatrix((int(*) [6]) vector); 

	/*Update our LTS */
	lsequence = max / 10;
	printf("Setting our LTS sequence to %d\n", lsequence);

	/*For debugging list chatrooms, and chats  loaded*/
	printf("Loaded from disk:\n");
	rooms = chatroomhead->next;

	while(rooms != NULL) {
		bzero(tempname, 25);
		printf("Room: %s\n", rooms->name);
		sprintf(tempname, "%s%d", rooms->name, atoi(server));
		SP_join(Mbox, tempname); /*Join the room so we get client updates*/
		struct node* i;
		i = rooms->head->next;

		while (i != NULL) {
			printf("|%u|", i->data->sequence);
			i = i->next;
		}

		printf("\n");
		rooms = rooms->next;
	}  
}

void send_liked_msg(struct node *msg, int received) { // The received field says whether the clients already have this chat_packet.
	struct chat_packet temp;
	struct likes *l = msg->likes->next;
	memcpy(&temp, msg->data, sizeof(struct chat_packet));
	strcat(temp.group, server); /*Append server id for server's group */
	/*count likes */
	while (l != NULL) {
		if (l->like == 1) temp.num_likes++;
		l = l->next;
	}
	printf("Sending text to group %s, message type: %d\n", temp.group, received);
	// Send a like update to the clients.
	if (received == 0) {
		SP_multicast(Mbox, AGREED_MESS, temp.group, 0, sizeof(struct chat_packet), (const char *) (&temp));
	} else if (received == 1) {
		SP_multicast(Mbox, AGREED_MESS, temp.group, 1, sizeof(struct chat_packet), (const char *) (&temp));
	} else if (received == 5) {
		// This happens only for chat_packets that are sent from a membership change. So the client will need to insert these in order of LTS.
		SP_multicast(Mbox, AGREED_MESS, temp.group, 13, sizeof(struct chat_packet), (const char *) (&temp));
	}
       else
	{
		printf("Uh oh, type is %d\n", received);
	}
}

void recv_like(struct chat_packet *c) {
	int 		i;
	struct node	*temp;

	i = c->server_id - 1;
	lsequence++;
	c->sequence = lsequence * 10 + atoi(server); // give the like package a sequence.

	array_ll_insert(c, i);

	write_data();

	temp = chatroom_process_like(c);

	/* Send the message to the group with the modified likes to the group*/
	send_liked_msg(temp, 0);

	/* Send message to servers */
	SP_multicast(Mbox, AGREED_MESS, "Servers", 0, sizeof(struct chat_packet), (const char *) c);

	/*Update vector */
	vector[atoi(server)][c->server_id] = c->sequence;
}
 
void recv_text(struct chat_packet *c) {
	int 		i;
	struct node	*temp;

	i = c->server_id - 1;
	lsequence++;
	c->sequence = lsequence * 10 + atoi(server); // give the like package a sequence.
	vector[atoi(server)][atoi(server)] = c->sequence; //Update our vector.

	array_ll_insert(c, i);

	write_data();

	temp = chatroom_insert_msg(c);

	/* Send message to servers */
	SP_multicast(Mbox, AGREED_MESS, "Servers", 0, sizeof(struct chat_packet), (const char *) c);

	/* Send the message to the group with the modified likes to the group*/
	send_liked_msg(temp, 1);
}

void recv_server_like(struct chat_packet *c, int16 mess_type) {
	struct chatrooms *r;
	struct node	 *temp, *temp2;
	struct likes	 *l;
	if (c->sequence > vector[atoi(server)][c->server_id] || mess_type == 5) {
		/*Update vector and lsequence*/
		if ((c->sequence > vector[atoi(server)][c->server_id])) {
			vector[atoi(server)][c->server_id] = c->sequence;
		}
		if (c->sequence > (lsequence * 10)) {
			lsequence = c->sequence / 10;
		}
	        Last_packets[c->server_id - 1]->next = (struct node *)calloc(1,sizeof(struct node));
	        Last_packets[c->server_id - 1]->next->data = (struct chat_packet *)calloc(1,sizeof(struct chat_packet));
	        memcpy(Last_packets[c->server_id - 1]->next->data, c, sizeof(struct chat_packet));
	        r = chatroomhead;

		// Find the correct group.
	        while (r->next != NULL && (strcmp(r->name, c->group) != 0))
	        {
	          r = r->next;
	        }
		if (strcmp(r->name, c->group) != 0) /*Chat room doesn't exist*/
	        {
	          r->next = (struct chatrooms *)calloc(1,sizeof(struct chatrooms));
	          r = r->next;
	          r->head = (struct node *)calloc(1,sizeof(struct node));
	          r->tail = r->head;
	          strncpy(r->name, c->group, strlen(c->group)); /*Copy name to chatroom */
	        }
	
		temp = r->head;
		while (temp->next != NULL) {
			if (c->lts == temp->next->data->sequence) {
				l = temp->next->likes;
				while (l->next != NULL) {
				  /* find the user who liked && check if the timestamp is larger */
				  if (strcmp(l->next->name, c->name) == 0) {
					if (l->next->like_timestamp > c->sequence) break; // Break if the like_timestamp is larger than the chat_packet's
					if (c->type == 7) {
					  l->next->like = 0;
					} else {
					  l->next->like = 1;
					}
					l->next->like_timestamp = c->sequence;
					
					break;
				  }
				  l = l->next;
				}
	
				if (l->next == NULL) {
				  l->next = (struct likes *)calloc(1,sizeof(struct likes));
				  l = l->next;
				  if (c->type == 7) {
				    l->like = 0;
				  } else {
				    l->like = 1;
				  }
				  l->like_timestamp = c->sequence;
				  strcpy(l->name, c->name);
				}
	
				break;
			}
			else if (c->lts < temp->next->data->sequence) { /*received a like for a nonreceived chat_packet so create a placeholder with node.exists = 0*/
				temp2 = temp->next;
				temp->next = (struct node *)calloc(1,sizeof(struct node));
				temp->next->data = (struct chat_packet *)calloc(1,sizeof(struct chat_packet));
				temp->next->data->sequence = c->lts; // This is the LTS of the chat_packet that is supposed to go here
				temp->next->exists = 0; // This chat_packet has not be received
				temp->next->next = temp2;
				temp->next->likes = (struct likes *)calloc(1,sizeof(struct likes)); // First "likes" is an empty head node
				l = temp->next->likes;
				l->next = (struct likes *)calloc(1,sizeof(struct likes));
				l = l->next;
				if (c->type == 7) {
				  l->like = 0;
				} else {
				  l->like = 1;
				}
				l->like_timestamp = c->sequence;
				strcpy(l->name, c->name);
				break;
			}
			temp = temp->next;
		}

		// If the message to like is nonexistant but it would be inserted at the end of the linked list:
		if (temp->next == NULL) {
			temp2 = temp->next;
			temp->next = (struct node *)calloc(1,sizeof(struct node));
			temp->next->data = (struct chat_packet *)calloc(1,sizeof(struct chat_packet));
			temp->next->data->sequence = c->lts; // This is the LTS of the chat_packet that is supposed to go here
			temp->next->exists = 0; // This chat_packet has not be received
			temp->next->next = temp2;
			temp->next->likes = (struct likes *)calloc(1,sizeof(struct likes)); // First "likes" is an empty head node
			l = temp->next->likes;
			l->next = (struct likes *)calloc(1,sizeof(struct likes));
			l = l->next;
			if (c->type == 7) {
			  l->like = 0;
			} else {
			  l->like = 1;
			}
			l->like_timestamp = c->sequence;
			strcpy(l->name, c->name);
		}
	
	        write_data();
	        Last_packets[c->server_id - 1] = Last_packets[c->server_id - 1]->next; /* Move pointer */
		if (temp->next->exists == 1) {
		        /* Send the message to the group with the modified likes to the group*/
			send_liked_msg(temp->next, 0);
		}
	}
	else
	{
		printf("Packet received already.\n");
	}
}

void recv_server_msg(struct chat_packet *c, int16 mess_type){ 
	struct chatrooms *r;
	struct node *temp, *temp2;
	char groupname[MAX_GROUP_NAME];

	if (mess_type == 5 && c->sequence <= vector[atoi(server)][c->server_id]) {
		// Do not process this packet because it came from a membership change but we already have this packet
		return;
	}
	/* If like message, process it as a like */
	if (c->type == 1 || c->type == 7) {
		printf("Received like for message: %d\n", c->lts);
		recv_server_like(c, mess_type);
		return;
	}
	/* Received username from server */
        else if (c->type == 10) {
	   /* Find room */
	   r = find_room(c->group);
	   printf("Received username change from other server name: %s, group %s\n", c->name, c->group);
           add_user(c->name, c->client_group, r->names, c->server_id);
           /* Inform any clients */
	   bzero(groupname, MAX_GROUP_NAME);
	   sprintf(groupname, "%s%d", c->group, atoi(server));
	 if (r->names == NULL)    printf("Rnames is null!\n");
	   send_namelist(groupname, r->names);
        }
        /* Recieved user leave from other server */
        else if (c->type == 11) {
	  printf("Received request to remove %s from group %s\n", c->client_group, c->group);
	   r = find_room(c->group);
           rm_user(c->client_group, r->names);
	   bzero(groupname, MAX_GROUP_NAME);
	   sprintf(groupname, "%s%d\0", c->group, atoi(server));
	 if (r->names == NULL)    printf("Rnames is null!\n");
	   send_namelist(groupname, r->names);
        }
        printf("Received text: %s\n", c->text);
   	/* Check to make sure we don't already have it */
	if (c->sequence > vector[atoi(server)][c->server_id] || mess_type == 5) {
		printf("Adding message %d, because our vector is %d\n", c->sequence, vector[atoi(server)][c->server_id]);
		/*Update vector and lsequence*/
		if (c->sequence > vector[atoi(server)][c->server_id]) {
			vector[atoi(server)][c->server_id] = c->sequence;
		}
		if (c->sequence > (lsequence * 10)) {
			lsequence = c->sequence / 10;
		}
	        Last_packets[c->server_id - 1]->next = (struct node *)calloc(1,sizeof(struct node));
	        Last_packets[c->server_id - 1]->next->data = (struct chat_packet *)calloc(1,sizeof(struct chat_packet));
	        memcpy(Last_packets[c->server_id - 1]->next->data, c, sizeof(struct chat_packet));
		// find the correct group (chatroom)
	        r = find_room(c->group);
		// find the correct spot to place the new message in the linked list. If a placeholder for the message is found then put it in there.
		temp = r->head;
		while (temp->next != NULL) {
			if (c->sequence < temp->next->data->sequence) {
				/*stitching */
				temp2 = temp->next;
				temp->next = (struct node *)calloc(1,sizeof(struct node));
				temp->next->data = (struct chat_packet *)calloc(1,sizeof(struct chat_packet));
				temp->next->likes = (struct likes *)calloc(1,sizeof(struct likes));
				temp->next->exists = 1;
				memcpy(temp->next->data, c, sizeof(struct chat_packet));
				temp->next->next = temp2;
				break;
			} else if (c->sequence == temp->next->data->sequence && temp->next->exists == 0) {
				temp->next->exists = 1;
				memcpy(temp->next->data, c, sizeof(struct chat_packet));
				break;
			}
			temp = temp->next;
		}
	
		if (temp->next == NULL) { /*Latest in sequence, put at end of list */
			temp->next = (struct node *)calloc(1,sizeof(struct node));
			temp->next->data = (struct chat_packet *)calloc(1,sizeof(struct chat_packet));
			temp->next->likes = (struct likes *)calloc(1,sizeof(struct likes));
			temp->next->exists = 1;
			memcpy(temp->next->data, c, sizeof(struct chat_packet));
			/* For sending below, set temp->next, since it is null */
			printf("Setting temp next\n");			
		}
	
	        write_data();
	        Last_packets[c->server_id - 1] = Last_packets[c->server_id - 1]->next; /* Move pointer */
	        /* Send the message to the group */
		if (mess_type == 0) {
		        send_liked_msg(temp->next, 1);
		} else if (mess_type == 5) {
			send_liked_msg(temp->next, 5);
		}
	}
	else
	{
		printf("Packet received already.\n");
	}

  /*Show the vector so we can compare it to other servers*/
  showmatrix((int(*) [6]) vector);
}

void recv_join_msg(struct chat_packet *c) {
  int ret;
  char groupname[MAX_GROUP_NAME];
	/* Add server index to groupname */
	strncpy(groupname, c->group, strlen(c->group));
        strcat(c->group, server);
	printf("Sending to client group %s\n", c->client_group);
        ret = SP_multicast(Mbox, AGREED_MESS, c->client_group, 3, sizeof(struct chat_packet), (const char *) c);
        /*send acknowledgement to the client's private group */
        if (ret < 0)
        {
                SP_error( ret );
        }
        /* Send room history to client */
        struct chat_packet temp;
        struct chatrooms *r;
        struct node *i;
	struct likes *l;
	struct names *n;
	char namelist[80];
	r = find_room(groupname);
        add_user(c->name, c->client_group, r->names, atoi(server));
 	/*Let other servers know the username was added */
        c->type = 10;
        bzero(c->group, MAX_GROUP_NAME);
	strncpy(c->group, groupname, strlen(groupname)); /*Remove serverid from group */
	c->server_id = atoi(server);
        ret = SP_multicast(Mbox, AGREED_MESS, "Servers", 3, sizeof(struct chat_packet), (const char *) c);
	
        i = r->head;
	n = r->names;
        while (i->next != NULL)
        {
	  if (i->next->exists != 0) {
		l = i->next->likes->next;
		memcpy(&temp, i->next->data, sizeof(struct chat_packet));
	        strcat(temp.group, server); /*Append server id for server's group */
		/*count likes */
		while (l != NULL) {
			if (l->like == 1) temp.num_likes++;
			l = l->next;
		}
		ret = SP_multicast(Mbox, AGREED_MESS, c->client_group, 3, sizeof(struct chat_packet), (const char *) (&temp));
	  }
          i = i->next;
          
        }
}

void update_userlist() /*Send all our users to the server group*/
{
	struct	chatrooms	*r;
	struct	names		*n;
	struct	pnames		*p;
	struct	chat_packet	c;

	r = chatroomhead;

	while (r->next != NULL) {
		n = r->next->names;

		while (n->next != NULL) {
			p = n->next->pnames;

			while (p->next != NULL) {
				c.type = 10;
				strncpy(c.name, n->next->name, strlen(n->next->name) + 1);
				strncpy(c.group, r->next->name, strlen(r->next->name) + 1);
				strncpy(c.client_group, p->next->pname, strlen(p->next->pname) + 1);
				c.server_id = p->next->server_id;

				SP_multicast(Mbox, AGREED_MESS, "Servers", 3, sizeof(struct chat_packet), (const char *) (&c));
			   p = p->next;
			}
		    n = n->next;
		}
	     r = r->next;
	}
}

void filter_userlist(int server_id) {
	struct	chatrooms	*r;
	struct	names		*n, *tempn;
	struct	pnames		*p, *tempp;

	r = chatroomhead;

	while (r->next != NULL) {
		n = r->next->names;

		while (n->next != NULL) {
			p = n->next->pnames;

			while (p->next != NULL) {
				printf("Looking for server %d found Server=%d\n", server_id, p->next->server_id);
				if (p->next->server_id == server_id) {
					printf("Removing %s\n", p->next->pname);
					p->next = p->next->next;
				}
				else {
	 			  p = p->next;
				}
			}

			if (n->next->pnames->next == NULL && n->next != NULL) { /*remove if not last in list */
				printf("Removing %s\n", n->next->name);
				n->next = n->next->next;
			  
			}
			else if (n->next->pnames->next == NULL && n->next == NULL) { /*Last in list, so just null next */
				printf("Removing last name in list %s\n", n->next->name);
				n->next = NULL;
 			}
		   if (n->next != NULL) n = n->next; /*due to last case, we may have already set n-next = null*/
		}
	     r = r->next;
	}
	
}

void send_namelist(char *group, struct names *names)
{
  char namelist[80];
  struct chat_packet *c;
  struct names *n;
  n=names;
  int ret;
  c = (struct chat_packet *)calloc(1,sizeof(struct chat_packet));
  
        bzero(namelist, 80);
        while (n->next != NULL) /*Build membership group name list */
        {
          strcat(namelist, n->next->name);
          strcat(namelist, " ");
 	  printf("added %s\n", n->next->name);
          n = n->next;
          
        }
        /*Send namelist to everyone in chatroom*/
        strncpy(c->text, namelist, strlen(namelist));
        c->type = 9;
	strncpy(c->group, group, strlen(group));
        ret = SP_multicast(Mbox, AGREED_MESS, group, 3, sizeof(struct chat_packet), (const char *) (c));

        printf("Sent names in chatroom: %s to group %s with return %d\n", c->text, group, ret);

}

void recv_client_msg(struct chat_packet *c) {
   int ret;
   printf("Got packet type %d\n", c->type);
   if (c->type == 0)
   {
	recv_text(c);
   }
   else if (c->type == 1 || c->type == 7) /* 7 means unlike, 1 means like */
   {
	recv_like(c);
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
   else if (c->type == 8) /* Request to view online servers */
   {
	int i;
	char text[3];
	for (i=1; i < 6; i++) 
	{
	  if (servers_online[i])
	  {
	    sprintf(text, "%d, ", i);
	    strcat(c->text, text);
	  }
	}
	/*trim trailing comma */
	c->text[strlen(c->text)-2]='\0';
	/* Send the string to the client */
	ret = SP_multicast(Mbox, AGREED_MESS, c->client_group, 3, sizeof(struct chat_packet), (const char *) c);
   }
}

void send_all_after(int min, int c)
{
	/*Send all after LTS here */
	int ret;
	struct node *i;
	i = Server_packets[c - 1].next;
	printf("Send lts %d, for server %d\n", min, c);
	
	while (i != NULL) {
		if (i->data->sequence > min) {
	   	    printf("Sending packet with LTS:%d\n", i->data->sequence);
	 	    ret = SP_multicast(Mbox, AGREED_MESS, "Servers", 5, sizeof(struct chat_packet), (const char *) i->data);

	   	    if (ret < 0) {
			SP_error( ret );
		    }
		}
		i = i->next;
	} 
}

void showmatrix(int rvector[][6])
{
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
	
}


void recv_update(int rvector[][6]) {
	int max, min, c, i, j, s;
	printf("Received vector\n");
	showmatrix((int(*) [6]) rvector);
	
	/*Determine if we have recevied all the updates */
	vectors_received++;
	printf("Comparing vrec %d with serv-avail %d\n", vectors_received, servers_available-1);
	if (vectors_received == servers_available -1) /*We got all the updates */
	{
	   /*Reset vector count */
	   vectors_received = 0;
	   printf("Got all updates after join, checking to see what we need to send\n");
	   printf("Current matrix:\n");
	
	   showmatrix((int(*) [6]) vector);
	
	   for (i = 1; i<6; i++)
	   {
	   if (servers_online[i]) /* Only calculate vectors from servers that are online */
           {
	     max=0;
	     min=vector[1][i]; /*Set min to first value */ printf("Min set to %d ", min);
	     for (j=1; j<6; j++)
	     {
	       if (servers_online[j]) /* Only calculate vectors from servers that are online */	
	       {
 	         if (vector[j][i] > max)
	  	  {
		   max = vector[j][i];
		   c = j;
		   printf("Max: %d ", max);
		  }
		  if (vector[j][i] < min)
	          {
		   min = vector[j][i]; printf("R%d\n", min);
		   s = j; /* Server who needs it */
	          }
 	       }
	     }
	    }
	     if (c == atoi(server)  && servers_online[i] && min < max) /*We have the latest, so send all after min LTS */
	     {
		send_all_after(min, i);printf("Sent all after %d for server %d\n", min, s);
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
void send_remove(char *pname, char *group)
{
  struct chat_packet *c;
  c = (struct chat_packet *)calloc(1, sizeof(struct chat_packet));
  c->type=11;
  strncpy(c->client_group, pname, strlen(pname));
  strncpy(c->group, group, strlen(group));
  printf("Sending removal of %s in group %s\n", c->client_group, c->group);
  SP_multicast(Mbox, AGREED_MESS, "Servers", 3,sizeof(struct chat_packet), (const char *) c);

}
void memb_change() {
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
        char             connect_group[2];
	char		roomname[25];
	struct 	chatrooms *r;
        service_type = 0;
        sprintf(connect_group, "c%s", server);

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
			  printf("Received message from another server: sender:%c our server:%c\n", sender[1], server[0]);
			  if (mess_type == 10) /*Vector update */
			  {
				recv_update((int(*) [6]) mess);
			  }
			  else /* Membership change mode when mess_type is 5 */
			  {
			  recv_server_msg((struct chat_packet *) mess, mess_type);
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
                                sender, num_groups, mess_type ); // Figure out what variable goes here
                        for( i=0; i < num_groups; i++ )
                                printf("\t%s\n", &target_groups[i][0] );
                        printf("grp id is %d %d %d\n",memb_info.gid.id[0], memb_info.gid.id[1], memb_info.gid.id[2] );

			if (strncmp(sender, "Servers", 7)==0)
                            {
                                servers_available = num_groups;  /*To determine how many servers to update */
                                printf("Running membership change\n");
                                /*Update servers online*/
                                printf("Serv online...numgrp=%d\n", num_groups);
				/* Zero out server array */
				for (i=1;i < 6; i++) servers_online[i] = 0;
				/* Update those online */
                                for (i=0; i < num_groups; i++)
                                {  printf("Server %c online\n", target_groups[i][1] );
                                  servers_online[atoi(&target_groups[i][1])] = 1;
                                }
                                memb_change();
				if ( Is_caused_join_mess( service_type))
				{
				/*New server joined, send our user list */
				  update_userlist();
				} else if (Is_caused_disconnect_mess( service_type ) || 
					   Is_caused_network_mess( service_type) ) {
					for (i=1; i < 6; i++)
			 		{
					  if (servers_online[i]==0) /* Server is offline, remove users */
					  {
						  printf("Filtering out users from server %d\n", i);
						  filter_userlist(i);
					  }
					}
				 	 /*Let client know. */
					send_allnames();
				}
                            }
			else if (strncmp(sender, connect_group, 2) == 0)
			{
				printf("client connect/disconnect\n");
			}
			else
			{
		          if( Is_caused_join_mess( service_type) )
			  {
			    /*User joined a group, user should be added already, send user update to group*/
			    /*Do update if we aren't joining */
			    if (strncmp(Private_group, memb_info.changed_member, strlen(Private_group))) { 
			      bzero(roomname, MAX_GROUP_NAME); 
  			      strncpy(roomname, sender, strlen(sender)-1); /*remove tacked on server id*/
			      r = find_room(roomname);
			      send_namelist(sender, r->names);
			     }
			  }
			  else if (Is_caused_leave_mess( service_type ) || Is_caused_disconnect_mess(service_type))
			 {
			    strncpy(roomname, sender, strlen(sender)-1);
			     r = find_room(roomname);
			    rm_user(memb_info.changed_member, r->names);
			    send_remove(memb_info.changed_member, roomname);
			    send_namelist(sender, r->names);
			 }
			
			}
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
  int i, j;
  int ret;
  char group[80];
  char messages[5];
  struct node first_node; // Sentinel node used for read_disk()
  chatroomhead = (struct chatrooms *) calloc(1, sizeof(struct chatrooms));
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
  for (i=0; i< 7; i++)
  {
    servers_online[i] = 0;
  }
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
  /* Join our connection group for client awareness*/
  sprintf(group, "c%s", argv[1]);
  ret = SP_join(Mbox, group);
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

/* Create a chatroom at the end of the linked list of chatrooms */
struct chatrooms * create_room(char *room_name, struct chatrooms *rooms_tail) {
	if (rooms_tail == NULL || rooms_tail->next != NULL) {
		printf("An error occurred with creating a new room, %s\n", room_name);

		return NULL;
	}
	char groupname[MAX_GROUP_NAME];
	bzero(groupname, MAX_GROUP_NAME);
	int ret;
	rooms_tail->next = (struct chatrooms *) calloc(1, sizeof(struct chatrooms));
	rooms_tail = rooms_tail->next;
	rooms_tail->head =  (struct node *) calloc(1,sizeof(struct node));
	//rooms_tail->head->next = NULL; we shouldn't need this since we changed to calloc
	rooms_tail->tail = rooms_tail->head;
	rooms_tail->names = (struct names *)calloc(1, sizeof(struct names));
        rooms_tail->names->pnames = (struct pnames *)calloc(1,sizeof(struct pnames));
	bzero(rooms_tail->name, MAX_GROUP_NAME);
	strncpy(rooms_tail->name, room_name, strlen(room_name));
	sprintf(groupname, "%s%d", room_name, atoi(server));
	ret = SP_join(Mbox, groupname); /* Join room so we can see client connects/disconnects */
        if (ret < 0) SP_error(ret);
        printf("Joined %s\n", groupname);
	return rooms_tail;
}

/* 
 *  Finds a chatroom based on the name of the room.
 *  If not found, creates a new one by calling creat_room().
 */
struct chatrooms * find_room(char *room_name) {
	struct chatrooms *temp = chatroomhead;

	while (temp->next != NULL) {
		if (strcmp(temp->next->name, room_name) == 0) {
			return temp->next;
		}
		temp = temp->next;
	}

	return create_room(room_name, temp);
}

void send_allnames() {
   struct chatrooms *temp = chatroomhead;
   while (temp->next != NULL) {
      send_namelist(temp->next->name, temp->next->names);
     temp = temp->next;
   }
}

/* 
 * Finds the spot where the message should be inserted to maintain order based on
 * lamport timestamp.
 */
struct node * find_insert_slot(int target_stamp, struct chatrooms *room) {
	struct node *temp = room->head;

	while (temp->next != NULL) {
		if (target_stamp <= temp->next->data->sequence) {
			return temp;
		}
		temp = temp->next;
	}
	return temp;
}

/* Creates an empty node used for chatrooms. */
struct node * empty_chatroom_node() {
	struct node *temp = (struct node *)calloc(1, sizeof(struct node));
	temp->data = (struct chat_packet *) calloc(1,sizeof(struct chat_packet));
	temp->likes = (struct likes *) calloc(1,sizeof(struct likes));
	temp->exists = 0;

	return temp;
}

/* Chatroom insert for a chat_packet. If the chat already exists, do nothing. */
struct node * chatroom_insert_msg(struct chat_packet *msg) {
	struct chatrooms	*room;
	struct node			*slot, *temp;

	if (strlen(msg->group) == 0) {
		printf("An error occurred due to a nonspecified room name");
		return NULL;
	}

	room = find_room(msg->group);
	if (room == NULL) {
		return NULL;
	}

	// Insert the new message
	slot = find_insert_slot(msg->sequence, room);
	if (slot->next != NULL && slot->next->sequence == msg->sequence) {
		printf("An error occured inserting a message into the chatroom data structure\n");
		return NULL;
	}

	if (slot->next == NULL || msg->sequence < slot->next->sequence) {
		temp = slot->next;
		slot->next = empty_chatroom_node();
		slot->next->next = temp;
	}

	slot->next->exists = 1;
	memcpy(slot->next->data, msg, sizeof(struct chat_packet));
	return slot->next;
}

/* Array linked list insert for a chat_packet */
void array_ll_insert(struct chat_packet *msg, int server) {
	// Set up the new node.
	Last_packets[server]->next = (struct node *)calloc(1,sizeof(struct node));
	Last_packets[server] = Last_packets[server]->next;
	Last_packets[server]->data = (struct chat_packet *)calloc(1,sizeof(struct chat_packet));
	Last_packets[server]->likes = (struct likes *)calloc(1,sizeof(struct likes));

	// Copy chat_packet into the new node.
	memcpy(Last_packets[server]->data, msg, sizeof(struct chat_packet));
}

/* Finds the message which is to be liked and returns the head of the likes */
struct node * find_desired_msg(int target_stamp, struct chatrooms *room) {
	struct node *temp, *temp2;
	temp = room->head;
	while (temp->next != NULL) {
		if (target_stamp == temp->next->data->sequence) {
			return temp->next;
		}
		if (target_stamp < temp->next->data->sequence) {
			break;
		}
		temp = temp->next;
	}

	temp2 = temp->next;
	temp->next = empty_chatroom_node();
	temp->next->data->sequence = target_stamp;
	return temp->next;
}

struct likes * find_user_like(char *username, struct likes *head) {
	struct likes *temp;
	temp = head;
	while (temp->next != NULL) {
		if (strcmp(username, temp->next->name) == 0) {
			return temp->next;
		}
		temp = temp->next;
	}

	temp->next = (struct likes *)calloc(1, sizeof(struct likes));
	strncpy(temp->next->name, username, strlen(username) + 1);
	return temp->next;
}

struct node * chatroom_process_like(struct chat_packet *msg) {
	struct chatrooms	*room;
	struct node			*target_msg;
	struct likes		*target_like;

	room = find_room(msg->group);
	if (room == NULL) {
		return;	
	}

	target_msg = find_desired_msg(msg->lts, room);
	if (target_msg == NULL) {
		return;
	}

	target_like = find_user_like(msg->name, target_msg->likes);
	if (msg->sequence > target_like->like_timestamp) {
		if (msg->type == 1) {
			target_like->like = 1;
		} else if (msg->type == 7) {
			target_like->like = 0;
		}
		target_like->like_timestamp = msg->sequence;
	}

	return target_msg;
}

// vim: set tabstop=4
