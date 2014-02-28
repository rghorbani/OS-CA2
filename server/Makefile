CC = gcc
CFLAGS = -g -O2
OBJECTS = P2P_server.o defines.o func_lib.o info.o

main.out : $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o server

%.o : %.c
	$(CC) $(CFLAGS) -c $<
