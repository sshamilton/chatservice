CC=gcc
DEPS = sp.h
#CFLAGS = -ansi -c -Wall -pedantic 
CFLAGS = -c  -g
SP_LIBRARY_DIR = /home/cs437/exercises/ex3
all: chat_client chat_server

chat_client: chat_client.o $(DEPS) 
	    $(CC) -o chat_client chat_client.o $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a 
chat_server: chat_server.o $(DEPS)
	    $(CC) -o chat_server chat_server.o $(SP_LIBRARY_DIR)/libspread-core.a -ldl -lm -lrt -lnsl $(SP_LIBRARY_DIR)/libspread-util.a
clean:
	rm *.o
	rm chat_client
	rm chat_server

%.o:    %.c
	$(CC) $(CFLAGS) $*.c

