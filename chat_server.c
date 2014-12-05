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
static  int servers_online[6];
void read_disk();
void send_vector();
void recv_client_msg();
void write_data();
void memb_change();
void showmatrix(int rvector[][6]);
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

/*
 * Read disk is used for recovering from crashes. Things to do:
 * 1. Restore data structure and recover data for users connected to the
 * server.
 * 2. Restore the vector.
 */
void read_disk() {
  int i, foundroom =0, max =0;
  struct chat_packet *c_temp;
  struct likes *l;
  struct chatrooms *rooms, *r;
  struct node *temp, *temp2, *temp3;
  c_temp = malloc(sizeof(struct chat_packet));
  while (fread(c_temp, sizeof(struct chat_packet), 1, fp) != 0) {
    rooms = chatroomhead;
    i = c_temp->server_id - 1;
    printf("Loaded message: %s, room: %s\n", c_temp->text, c_temp->group);
    // Allocate memory for the next node.
    Last_packets[i]->next = (struct node *) malloc(sizeof(struct node));

    // Update the pointer to new tail node.
    Last_packets[i] = Last_packets[i]->next;
    Last_written[i] = Last_written[i]->next;

    // Allocate memory for the new node's data.
    Last_packets[i]->data = (struct chat_packet *) malloc(sizeof(struct chat_packet));

    // memcpy the read data into the newly allocated memory for the next node.
    memcpy(Last_packets[i]->data, c_temp, sizeof(struct chat_packet));
    /* If sequence is higher, update our max*/
    if (c_temp->sequence > max) max = c_temp->sequence;
    /* In this while loop of reading from file, we only populate the chatroom data structure
     * if the chat_packet we are reading is a chat message (not a like) -- do likes after*/
    if (c_temp->type != 1) {
	    while (rooms->next != NULL)
	    {
	       rooms = rooms->next;
	       if (strncmp(rooms->name, c_temp->group, strlen(c_temp->group)) == 0)
		{
		  /*chatroom exists*/
		  foundroom = 1; printf("Found chatroom %s\n", rooms->name);
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
	       printf("Created chatroom %s", rooms->name);
	    }
            foundroom = 0; /*Reset found room for next iteration */	
	    temp = rooms->head; /* Go to first chat in chatroom */
	   printf("Adding %d to room: %s\n", c_temp->sequence, rooms->name);
	    while (temp->next != NULL) { /*Search for correct placement of the message.  This provides correct ordering */
	       if (c_temp->sequence < temp->next->data->sequence) {
		 temp2 = temp->next;
		 temp->next = malloc(sizeof(struct node));
		 temp->next->data = malloc(sizeof(struct chat_packet));
		 temp->next->likes = malloc(sizeof(struct likes));
		 temp->next->exists = 1;
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
		rooms->tail->likes = malloc(sizeof(struct likes));
		rooms->tail->exists = 1;
		memcpy(rooms->tail->data, c_temp, sizeof(struct chat_packet));
	    }
    }
    /* Update our LTS vector */
    if (vector[c_temp->server_id][Last_packets[i]->data->server_id] < Last_packets[i]->data->sequence)
    {
      vector[c_temp->server_id][Last_packets[i]->data->server_id] = Last_packets[i]->data->sequence;
	printf("Updated vector %d %d to %d\n", c_temp->server_id, Last_packets[i]->data->server_id, Last_packets[i]->data->sequence);
    }
    if (vector[atoi(server)][Last_packets[i]->data->server_id] < Last_packets[i]->data->sequence)
    {
      vector[atoi(server)][Last_packets[i]->data->server_id] = Last_packets[i]->data->sequence;
        printf("Updated vector %d %d to %d\n", c_temp->server_id, Last_packets[i]->data->server_id, Last_packets[i]->data->sequence);
    }

    // Clear the new node's next pointer for safety.
    Last_packets[i]->next = NULL;

  }
  printf("Vector after load:\n");
  showmatrix((int(*) [6]) vector); 
  r = chatroomhead;
  /* Loop for likes. Can make more efficient by doing this in the previous loop */
  for (i = 0; i < 5; i++) {
    temp3 = &Server_packets[i];
    while (temp3->next != NULL) {
      if (temp3->next->data->type == 1) {
		// PUT THE LIKE IN THE CHATROOM DATA STRUCTURE
		c_temp = temp3->next->data;
	        while (r->next != NULL && (strncmp(r->name, c_temp->group, strlen(c_temp->group)) != 0))
	        {
	          r = r->next;
	        }
		if (strncmp(r->name, c_temp->group, strlen(c_temp->group)) != 0) /*Chat room doesn't exist*/
	        {
	          r->next = malloc(sizeof(struct chatrooms));
	          r = r->next;
	          r->head = malloc(sizeof(struct node));
	          r->tail = r->head;
	          strncpy(r->name, c_temp->group, strlen(c_temp->group)); /*Copy name to chatroom */
	        }
	
		temp = r->head;
		while (temp->next != NULL) {
			if (c_temp->lts == temp->next->data->sequence) {
				l = temp->next->likes;
				while (l->next != NULL) {
				  /* find the user who liked && check if the timestamp is larger */
				  if (strncmp(l->next->name, c_temp->name, strlen(c_temp->name)) == 0) {
					if (l->next->like_timestamp > c_temp->sequence) break; // Break if the like_timestamp is larger than the chat_packet's
					if (c_temp->type == 7) {
					  l->next->like = 0;
					} else {
					  l->next->like = 1;
					}
					l->next->like_timestamp = c_temp->sequence;
					
					break;
				  }
				  l = l->next;
				}
	
				if (l->next == NULL) {
				  l->next = malloc(sizeof(struct likes));
				  l = l->next;
				  if (c_temp->type == 7) {
				    l->like = 0;
				  } else {
				    l->like = 1;
				  }
				  l->like_timestamp = c_temp->sequence;
				  strcpy(l->name, c_temp->name);
				}
	
				break;
			}
			else if (c_temp->lts < temp->next->data->sequence) { /*received a like for a nonreceived chat_packet so create a placeholder with node.exists = 0*/
				temp2 = temp->next;
				temp->next = malloc(sizeof(struct node));
				temp->next->data = malloc(sizeof(struct chat_packet));
				temp->next->data->sequence = c_temp->lts; // This is the LTS of the chat_packet that is supposed to go here
				temp->next->exists = 0; // This chat_packet has not be received
				temp->next->next = temp2;
				temp->next->likes = malloc(sizeof(struct likes)); // First "likes" is an empty head node
				l = temp->next->likes;
				l->next = malloc(sizeof(struct likes));
				l = l->next;
				if (c_temp->type == 7) {
				  l->like = 0;
				} else {
				  l->like = 1;
				}
				l->like_timestamp = c_temp->sequence;
				strcpy(l->name, c_temp->name);
				break;
			}
			temp = temp->next;
		}

		if (temp->next == NULL) {
			temp2 = temp->next;
			temp->next = malloc(sizeof(struct node));
			temp->next->data = malloc(sizeof(struct chat_packet));
			temp->next->data->sequence = c_temp->lts; // This is the LTS of the chat_packet that is supposed to go here
			temp->next->exists = 0; // This chat_packet has not be received
			temp->next->next = temp2;
			temp->next->likes = malloc(sizeof(struct likes)); // First "likes" is an empty head node
			l = temp->next->likes;
			l->next = malloc(sizeof(struct likes));
			l = l->next;
			if (c_temp->type == 7) {
			  l->like = 0;
			} else {
			  l->like = 1;
			}
			l->like_timestamp = c_temp->sequence;
			strcpy(l->name, c_temp->name);
		}

      }
      temp3 = temp3->next;
    }	
  }

    /*Update our LTS */
    lsequence = (max)/10;
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
	if (received == 1) {
	        SP_multicast(Mbox, AGREED_MESS, temp.group, 5, sizeof(struct chat_packet), (const char *) (&temp));
	} else if (received == 3) {
	        SP_multicast(Mbox, AGREED_MESS, temp.group, 3, sizeof(struct chat_packet), (const char *) (&temp));
	} else if (received == 5) {
		// This happens only for chat_packets that are sent from a membership change. So the client will need to insert these in order of LTS.
	        SP_multicast(Mbox, AGREED_MESS, temp.group, 13, sizeof(struct chat_packet), (const char *) (&temp));
	}
}

void recv_like(struct chat_packet *c) {
	struct chatrooms *r;
	struct node	 *temp;
	struct likes	 *l;
	lsequence++;
	c->sequence = lsequence*10 + atoi(server); // give the like package a sequence.
        Last_packets[atoi(server) - 1]->next = malloc(sizeof(struct node));
        Last_packets[atoi(server) - 1]->next->data = malloc(sizeof(struct chat_packet));
        memcpy(Last_packets[atoi(server) - 1]->next->data, c, sizeof(struct chat_packet));

        r = chatroomhead;
	// Find the correct group.
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

	temp = r->head->next;
	while (temp != NULL) {
		if (c->lts == temp->data->sequence) {
			l = temp->likes; // Head like is empty
			while (l->next != NULL) {
			  if (strncmp(l->next->name, c->name, strlen(c->name)) == 0) {
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
			  l->next = malloc(sizeof(struct likes));
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
		temp = temp->next;
	}

        write_data();
        Last_packets[c->server_id - 1] = Last_packets[c->server_id - 1]->next; /* Move pointer */
        /* Send the message to the group with the modified likes to the group*/
	send_liked_msg(temp, 1);
	/* Send message to servers */
	SP_multicast(Mbox, AGREED_MESS, "Servers", 3, sizeof(struct chat_packet), (const char *) c);
	/*Update vector */
	vector[atoi(server)][c->server_id] = c->sequence;
}
 
void recv_text(struct chat_packet *c) {
/*Received text message*/
        struct chatrooms *r;
        printf("Received text: %s\n", c->text);
        lsequence++;
        c->sequence = lsequence*10 + atoi(server); /*Lamport timestamp */
	c->num_likes = 0;

	/* Insert into the array-linkedlist data structure */
        Last_packets[atoi(server) - 1]->next = malloc(sizeof(struct node));
        Last_packets[atoi(server) - 1]->next->data = malloc(sizeof(struct chat_packet));
        memcpy(Last_packets[atoi(server) - 1]->next->data, c, sizeof(struct chat_packet));

	/* Insert into the chatrooms data structure */
        r = chatroomhead;
        while (r->next != NULL && (strncmp(r->name, c->group, strlen(c->group)) != 0))
        {
          r = r->next;
        }
	r->tail->next = malloc(sizeof(struct node));
	r->tail->next->data = malloc(sizeof(struct chat_packet));
	r->tail->next->likes = malloc(sizeof(struct likes));
	r->tail->next->exists = 1;
	memcpy(r->tail->next->data, c, sizeof(struct chat_packet));
	r->tail = r->tail->next;

        write_data();
        Last_packets[atoi(server) - 1] = Last_packets[atoi(server) - 1]->next; /* Move pointer */
        strcat(c->group, server); /*Append server id for server's group */
        printf("Sending text to group %s\n", c->group);
        /* Send the message to the group */
        SP_multicast(Mbox, AGREED_MESS, c->group, 3, sizeof(struct chat_packet), (const char *) c);
	/* Send message to servers */
	c->group[strlen(c->group) - 1] = '\0'; /*Remove the trailing server id*/
	SP_multicast(Mbox, AGREED_MESS, "Servers", 3, sizeof(struct chat_packet), (const char *) c);

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
	        Last_packets[c->server_id - 1]->next = malloc(sizeof(struct node));
	        Last_packets[c->server_id - 1]->next->data = malloc(sizeof(struct chat_packet));
	        memcpy(Last_packets[c->server_id - 1]->next->data, c, sizeof(struct chat_packet));
	        r = chatroomhead;

		// Find the correct group.
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
	
		temp = r->head;
		while (temp->next != NULL) {
			if (c->lts == temp->next->data->sequence) {
				l = temp->next->likes;
				while (l->next != NULL) {
				  /* find the user who liked && check if the timestamp is larger */
				  if (strncmp(l->next->name, c->name, strlen(c->name)) == 0) {
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
				  l->next = malloc(sizeof(struct likes));
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
				temp->next = malloc(sizeof(struct node));
				temp->next->data = malloc(sizeof(struct chat_packet));
				temp->next->data->sequence = c->lts; // This is the LTS of the chat_packet that is supposed to go here
				temp->next->exists = 0; // This chat_packet has not be received
				temp->next->next = temp2;
				temp->next->likes = malloc(sizeof(struct likes)); // First "likes" is an empty head node
				l = temp->next->likes;
				l->next = malloc(sizeof(struct likes));
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
			temp->next = malloc(sizeof(struct node));
			temp->next->data = malloc(sizeof(struct chat_packet));
			temp->next->data->sequence = c->lts; // This is the LTS of the chat_packet that is supposed to go here
			temp->next->exists = 0; // This chat_packet has not be received
			temp->next->next = temp2;
			temp->next->likes = malloc(sizeof(struct likes)); // First "likes" is an empty head node
			l = temp->next->likes;
			l->next = malloc(sizeof(struct likes));
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
			send_liked_msg(temp->next, 1);
		}
	}
	else
	{
		printf("Packet received already.\n");
	}
}

void recv_server_msg(struct chat_packet *c, int16 mess_type){ // NEED TO IMPLEMENT FILLING IN A MISSING CHAT_PACKET WHEN EXISTS == 0
	struct chatrooms *r;
	struct node *temp, *temp2;

	if (mess_type == 5 && c->sequence <= vector[atoi(server)][c->server_id]) {
		// Do not process this packet because it came from a membership change but we already have this packet
		return;
	}
	/* If like message, process it as a like */
	if (c->type == 1) {
		printf("Received like for message: %d\n", c->lts);
		recv_server_like(c, mess_type);
		return;
	}
        printf("Received text: %s\n", c->text);
   	/* Check to make sure we don't already have it */
	if (c->sequence > vector[atoi(server)][c->server_id] || mess_type == 5) {
		printf("Adding message %d, because our vetor is %d\n", c->sequence, vector[atoi(server)][c->server_id]);
		/*Update vector and lsequence*/
		if (c->sequence > vector[atoi(server)][c->server_id]) {
			vector[atoi(server)][c->server_id] = c->sequence;
		}
		if (c->sequence > (lsequence * 10)) {
			lsequence = c->sequence / 10;
		}
	        Last_packets[c->server_id - 1]->next = malloc(sizeof(struct node));
	        Last_packets[c->server_id - 1]->next->data = malloc(sizeof(struct chat_packet));
	        memcpy(Last_packets[c->server_id - 1]->next->data, c, sizeof(struct chat_packet));
		// find the correct group (chatroom)
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
		// find the correct spot to place the new message in the linked list. If a placeholder for the message is found then put it in there.
		temp = r->head;
		while (temp->next != NULL) {
			if (c->sequence < temp->next->data->sequence) {
				/*stitching */
				temp2 = temp->next;
				temp->next = malloc(sizeof(struct node));
				temp->next->data = malloc(sizeof(struct chat_packet));
				temp->next->likes = malloc(sizeof(struct likes));
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
			r->tail->next = malloc(sizeof(struct node));
			r->tail = r->tail->next;
			r->tail->data = malloc(sizeof(struct chat_packet));
			r->tail->likes = malloc(sizeof(struct likes));
			r->tail->exists = 1;
			memcpy(r->tail->data, c, sizeof(struct chat_packet));
			/* For sending below, set temp->next, since it is null */
			printf("Setting temp next\n");
			temp->next = r->tail;
		}
	
	        write_data();
	        Last_packets[c->server_id - 1] = Last_packets[c->server_id - 1]->next; /* Move pointer */
	        /* Send the message to the group */
		if (mess_type == 3) {
		        send_liked_msg(temp->next, 3);
		} else if (mess_type == 5) {
			send_liked_msg(temp->next, 5);
		}
	}
	else
	{
		printf("Packet received already.\n");
	}

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
        struct chat_packet temp;
        struct chatrooms *r;
        struct node *i;
	struct likes *l;
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
	  if (i->exists != 0) {
		l = i->likes->next;
		memcpy(&temp, i->data, sizeof(struct chat_packet));
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
	     max=0;
	     min=vector[1][i]; /*Set min to first value */ printf("Min set to %d ", min);
	     for (j=1; j<6; j++)
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
	
	     if (c == atoi(server)) /*We have the latest, so send all after min LTS */
	     {
		send_all_after(min, i);printf("Sent all after %d for server %d\n", min, s);
	     }
	   }
	
	 printf("Final vector:\n");
	 showmatrix((int(*) [6]) vector);
	
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
   if (lsequence > 0)
   {
     vector[atoi(server)][atoi(server)] = lsequence *10 + atoi(server);
   }
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

                        if( Is_caused_join_mess( service_type ) )
                        {
                                printf("Due to the JOIN of %s\n", memb_info.changed_member );
                                // Deal with join of new member by joining the member's
                                // private 
                            if (strncmp(sender, "Servers", 7)==0)
			    {
				servers_available = num_groups;  /*To determine how many servers to update */
				printf("Running membership change\n");
				/*Update servers online*/
				/*Clear out array */
				for (i=0; i< 7; i++)
			        {
				    servers_online[i] = 0;
  				}
				printf("Serv online...numgrp=%d\n", num_groups);
				for (i=0; i < num_groups; i++)
				{  printf("Server %c online\n", target_groups[i][1] );
				  servers_online[atoi(&target_groups[i][1])] = 1;
				}
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

