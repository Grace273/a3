CFLAGS = -Wall -g

OBJ = main.o client.o channel.o

all: server

server: $(OBJ)
	gcc $(CFLAGS) -o server $(OBJ)

main.o: main.c client.h channel.h config.h
	gcc $(CFLAGS) -c main.c

client.o: client.c client.h channel.h config.h
	gcc $(CFLAGS) -c client.c

channel.o: channel.c channel.h config.h
	gcc $(CFLAGS) -c channel.c

clean:
	rm -f *.o server