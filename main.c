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

typedef struct Client
{
	int fd;
	char username[32];
} Client;

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

	// TODO: this is not working
	while (username == NULL)
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

	// TODO: create a linked list so we can store any number of clients and then turn the for loop into a while(1) loop
	Client clients[2];
	int client_socket;
	for (int i = 0; i < 2; i++)
	{
		printf("Listening for connections...\n");
		client_socket = accept_connection(listen_soc);
		printf("Client connected!\n");

		// add client to the array
		clients[i].fd = client_socket;
		// call helper so client can make up a username
		char *name = client_login(client_socket);
		strncpy(clients[i].username, name, sizeof(clients[i].username));
	}

	return 0;
}
