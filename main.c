#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h> // sockaddr_in, htons()
#include <sys/socket.h> // AF_INET, SOCK_STREAM, socket()
#include <arpa/inet.h>	// for mac
#include <unistd.h>		// write()
#include "client.h"
#include "channel.h"
#include "config.h"

#define PORT 54134
#define MAX_QUEUE 5

int accept_connection(int listen_soc, Client *clients)
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

	// find an empty slot
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (clients[i].fd == -1)
		{
			clients[i].fd = client_socket;
			clients[i].username[0] = '\0'; // start off with empty username
			break;
		}
	}

	return client_socket;
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

	// reuse the port immediately
	int opt = 1;
	if (setsockopt(listen_soc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
	}

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

	// prepare for connections
	Client clients[64];
	Channel channels[MAX_CHANNELS];
	int num_clients = 0;

	// mark every slot as -1 to signify it as currently empty and no channel and no target client for private message
	memset(channels, 0, sizeof(channels));
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		clients[i].fd = -1;
		clients[i].channel = -1;
		clients[i].dm_target = NULL;
	}

	// Keep track of open file descriptors (listen_soc, each client's fd)
	fd_set master_set;
	FD_ZERO(&master_set);
	FD_SET(listen_soc, &master_set);
	int max_fd = listen_soc;
	char buf[MAX_BUF];
	// char msg[MAX_BUF];

	while (1)
	{
		// Select blocks until at least one fd ready, returns and tells you which fds are ready
		fd_set read_fds = master_set;

		if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
		{
			perror("select");
			break;
		}

		// new client is connecting
		if (FD_ISSET(listen_soc, &read_fds))
		{
			if (num_clients >= MAX_CLIENTS)
			{
				// briefly accept to get the fd so we can notify and close it
				int tmp = accept(listen_soc, NULL, NULL);
				if (tmp != -1){
					write(tmp, "Server is full. Try again later.\n", 33);
					close(tmp);
				}
			}
			else
			{
				int new_fd = accept_connection(listen_soc, clients);
				if (new_fd == -1)
				{
					// If this fails, not a major issue for the server. So only skip this client's connection
					// Keep server running
					fprintf(stderr, "Failed to accept connection.");
				}

				else if (prompt_login(new_fd) == -1)
				{
					// If this fails, means that only an individual client failed.
					// Only close the CLIENT's fd and keep server running.
					fprintf(stderr, "prompt_login");
					close(new_fd);
					// clean up the slot that accept_connection filled in
					for (int i = 0; i < MAX_CLIENTS; i++)
					{
						if(clients[i].fd == new_fd)
						{
							clients[i].fd = -1;
							break;
						}
					}
				}

				else
				{
					num_clients++;
					FD_SET(new_fd, &master_set);
					if (new_fd > max_fd)
					max_fd = new_fd;
				}
			}
			
		}

		// check each client for incoming data
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			// empty, no client
			if (clients[i].fd == -1)
				continue;

			if (FD_ISSET(clients[i].fd, &read_fds))
			{
				int n = read(clients[i].fd, buf, sizeof(buf) - 1);
				if (n > 0)
				{
					buf[n] = '\0'; // ensures null termination
				}

				// client disconnected
				if (n <= 0)
				{
					FD_CLR(clients[i].fd, &master_set);

					if (remove_client(&clients[i]) == -1)
					{
						// Cleaning up client fails
						// Close its fd to prevent leaking client's fd 
						fprintf(stderr, "remove_client");
						close(clients[i].fd);
						clients[i].fd = -1;
					}

					num_clients--;
				}
				else
				{

					if (handle_client_message(n, buf, channels, clients, &clients[i]) == -1)
					{
						// Only this client's message encountered an error
						// Remove this client, keep server running
						FD_CLR(clients[i].fd, &master_set);
						remove_client(&clients[i]);
						num_clients--;
					}
				}
			}
		}
	}

	return 0;
}
