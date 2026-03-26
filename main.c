#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h> // sockaddr_in, htons()
#include <sys/socket.h> // AF_INET, SOCK_STREAM, socket()
#include <arpa/inet.h>	// for mac
#include <unistd.h>		// write()

#define PORT 54134
#define MAX_QUEUE 5
#define MAX_BUF 128
#define MAX_CLIENTS 64
#define MAX_CHANNELS 16
#define MAX_CHANNEL_NAME 32

typedef struct Channel {
    char name[MAX_CHANNEL_NAME];
    int active;		// 1 if channel is active, 0 if slot is empty
} Channel;

typedef struct Client
{
	int fd;
	char username[32];
	int channel; // index into channels[], -1 = no channel
} Client;

Channel channels[MAX_CHANNELS];	// global channels array

int accept_connection(int listen_soc)
{
	// just following last week's worksheet
	struct sockaddr_in client_addr;
	unsigned int client_len = sizeof(struct sockaddr_in);
	client_addr.sin_family = AF_INET;

	int client_socket = accept(listen_soc, (struct sockaddr *)&client_addr, &client_len);
	if (client_socket == -1)
	{
		perror("accept");
		return -1;
	}

	return client_socket;
}

char *read_username(int client_fd)
{
	static char buffer[32];
	int n = read(client_fd, buffer, sizeof(buffer) - 1);

	if (n <= 0)
	{
		return NULL;
	}

	buffer[n] = '\0'; // null terminate

	// remove newline if present
	char *newline = strchr(buffer, '\n');
	if (newline)
		*newline = '\0';

	return buffer;
}

char *client_login(int client_fd)
{
	char msg[MAX_BUF];

	// write welcome message
	sprintf(msg, "Welcome. Create a username to start chatting.\r\n");
	write(client_fd, msg, strlen(msg));

	// call helper
	char *username = read_username(client_fd);

	// check if null username
	while (username[0] == '\0')
	{
		sprintf(msg, "No username. Try Again.\r\n");
		write(client_fd, msg, strlen(msg));
		username = read_username(client_fd);
	}

	// confirm login success to user
	sprintf(msg, "Ok, %s!!\r\n", username);
	write(client_fd, msg, strlen(msg));

	return username;
}

// returns index of channel, or -1 if full
int find_or_create_channel(const char *name) {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].active && strcmp(channels[i].name, name) == 0)	// found existing
            return i;
    }
    // create new channel
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (!channels[i].active) {
            strncpy(channels[i].name, name, MAX_CHANNEL_NAME - 1);
            channels[i].active = 1;
            return i;
        }
    }
    return -1; // full
}

int main()
{
	// create listen soc for server
	int listen_soc = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_soc < 0)
	{
		perror("socket");
		exit(1);
	}

	// initialize server address
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(PORT);
	server.sin_addr.s_addr = INADDR_ANY;
	memset(&server.sin_zero, 0, 8);

	printf("Server is listening on port %d\n", PORT);

	// bind server to listen_soc
	if (bind(listen_soc, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == -1)
	{
		perror("bind");
		close(listen_soc);
		exit(1);
	}

	// set up queue to hold pending connections
	if (listen(listen_soc, MAX_QUEUE) < 0)
	{
		perror("listen");
		exit(1);
	}


	Client clients[64];
	int num_clients = 0; 

	// mark every slot as -1 to signify it as currently empty and no channel
	memset(channels, 0, sizeof(channels));
	for (int i = 0; i < MAX_CLIENTS; i++) {
		clients[i].fd = -1;
		clients[i].channel = -1;
	}

	// Keep track of open file descriptors (listen_soc, each client's fd)
	fd_set master_set; 
	FD_ZERO(&master_set);
	FD_SET(listen_soc, &master_set);
	int max_fd = listen_soc;

	while(1)
	{
		// Select blocks until at least one fd ready, returns and tells you which fds are ready 
		fd_set read_fds = master_set;

		if( select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
		{
			perror("select");
			break;
		}

		// new client is connecting 
		if (FD_ISSET(listen_soc, &read_fds))
		{
			int new_fd = accept_connection(listen_soc);

			// find an empty slot
			for (int i = 0; i < 64; i++)
			{
				if (clients[i].fd == -1) 
				{
					clients[i].fd = new_fd;
					clients[i].username[0] = '\0'; // start off with empty username
					num_clients++;
					break;
				}
			}

			FD_SET(new_fd, &master_set);
			if (new_fd > max_fd)
				max_fd = new_fd;
			
			write(new_fd, "Enter username:\r\n", 17);
		}

		// check each client for incoming data
		for (int i = 0; i < 64; i++)
		{
			// empty, no client 
			if (clients[i].fd == -1) continue;

			if (FD_ISSET(clients[i].fd, &read_fds))
			{
				char buf[MAX_BUF];
				int n = read(clients[i].fd, buf, sizeof(buf) - 1);
			
				// client disconnected
				if (n <= 0)
				{
					printf("%s disconected\n", clients[i].username);
					FD_CLR(clients[i].fd, &master_set);
					close(clients[i].fd);
					clients[i].fd = -1;
					clients[i].username[0] = '\0';
					num_clients--;
				}
				else
				{
					buf[n] = '\0';
					char *n1 = strchr(buf, '\n');
					if (n1) *n1 = '\0';

					if (clients[i].username[0] == '\0')
					{
						strncpy(clients[i].username, buf, 31);
						char msg[MAX_BUF];
						snprintf(msg, sizeof(msg), "Welcome, %s!\r\n", clients[i].username);
						write(clients[i].fd, msg, strlen(msg));
					}
					else	// chat message
					{
						char msg[MAX_BUF];

						if (buf[0] == '/') {
						// it's a command
						char cmd[32], arg[64];
						int parsed = sscanf(buf + 1, "%31s %63s", cmd, arg);

						if (strcmp(cmd, "join") == 0 && parsed == 2) {
							int ch = find_or_create_channel(arg);
							if (ch == -1) {
								write(clients[i].fd, "Error: too many channels.\r\n", 26);
							} else {
								clients[i].channel = ch;
								snprintf(msg, sizeof(msg), "Joined #%s\r\n", arg);
								write(clients[i].fd, msg, strlen(msg));
							}

						} else if (strcmp(cmd, "leave") == 0) {
							clients[i].channel = -1;
							write(clients[i].fd, "Left channel.\r\n", 15);

						} else {
							write(clients[i].fd, "Unknown command.\r\n", 18);
						}
						
					} else {		// regular message - broadcast to channel
						if (clients[i].channel == -1) {
            				write(clients[i].fd, "Join a channel first. Use /join <name>\r\n", 39);
       					} else {					
							snprintf(msg, sizeof(msg), "[#%s] %s: %s\r\n", 
							channels[clients[i].channel].name, 
							clients[i].username, buf);		// format message with channel and username

							for(int j = 0; j < MAX_CLIENTS; j++)
							{
								if (clients[j].fd != -1 
									&& clients[j].fd != clients[i].fd
									&& clients[j].channel == clients[i].channel)	// only send to clients in same channel
								{
									write(clients[j].fd, msg, strlen(msg));
								}
							}
						}
					}	
					}
				}
			}
		}
	}

	return 0;
}
