#ifndef CLIENT_H
#define CLIENT_H

typedef struct Client
{
    int fd;
    char username[32];
    int channel;              // index into channels[], -1 = no channel
    struct Client *dm_target; // targeted user for private message, NULL = no target
} Client;

// forward declaration to refer to Channel in function declarations
typedef struct Channel Channel;

int prompt_login(int client_fd);
int remove_client(Client *client);
int handle_client_message(int bytes_read, char *buf, Channel *channel_arr, Client *client_arr, Client *client);

#endif