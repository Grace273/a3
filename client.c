#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "client.h"
#include "channel.h"
#include "config.h"

#define MAX_MSG_BUF 300

int prompt_login(int client_fd)
{
    if (write(client_fd, "Enter user name:\r\n", 19) == -1)
    {
        perror("write");
        return -1;
    }

    return 0;
}

// returns a client pointer with arg username exists, NULL otherwise
// helper for join_dm
Client *find_client(char *arg, Client *clients)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].username[0] != '\0' && strcmp(clients[i].username, arg) == 0)
            return &clients[i];
    }

    return NULL;
}

// helper for handle_command
int join_channel(char *arg, Channel *channels, Client *client)
{
    char msg[MAX_MSG_BUF];

    // remove CR/LF from arg
    arg[strcspn(arg, "\r\n")] = '\0';

    int ch = find_or_create_channel(arg, channels);
    if (ch == -1)
    {
        if (write(client->fd, "Error: too many channels.\r\n", 26) == -1)
        {
            perror("write");
            return -1;
        }
        return -1;
    }

    client->channel = ch;
    client->dm_target = NULL;

    snprintf(msg, MAX_MSG_BUF, "----Joined #%s----\n(You can't see previous messages)\n"
                               "/who to see who's in the server\n"
                               "/leave to leave\nChat:\r\n",
             arg);

    if (write(client->fd, msg, strlen(msg)) == -1)
    {
        perror("write");
        return -1;
    }

    return 0;
}

// helper for handle_command
int join_dm(char *arg, Client *clients, Client *client)
{
    char msg[MAX_MSG_BUF];

    // remove CR/LF from arg
    arg[strcspn(arg, "\r\n")] = '\0';
    Client *target_client = find_client(arg, clients);

    if (target_client == NULL)
    {
        snprintf(msg, MAX_MSG_BUF, "No such user online.\r\n");
        if (write(client->fd, msg, strlen(msg)) == -1)
        {
            perror("write");
            return -1;
        }
    }
    else if (strcmp(target_client->username, client->username) == 0)
    {
        snprintf(msg, MAX_MSG_BUF, "You can't message yourself.\r\n");
        if (write(client->fd, msg, strlen(msg)) == -1)
        {
            perror("write");
            return -1;
        }
    }
    else
    {
        client->channel = -1;
        client->dm_target = target_client;

        snprintf(msg, MAX_MSG_BUF, "\n----Private Message with: %s----\n(You can't see previous messages)\n"
                                   "/leave to leave\nChat:\r\n",
                 arg);
        if (write(client->fd, msg, strlen(msg)) == -1)
        {
            perror("write");
            return -1;
        }
    }

    return 0;
}

// helper for handle_command

// helper for handle_client_message
int handle_command(char *buf, Channel *channel_arr, Client *clients, Client *client)
{
    char msg[MAX_MSG_BUF];
    char cmd[32] = {0}, arg[64] = {0};
    int parsed = sscanf(buf + 1, "%31s %63s", cmd, arg);

    if (strcmp(cmd, "join") == 0 && parsed == 2)
    {
        if (join_channel(arg, channel_arr, client) == -1)
        {
            return -1;
        }
    }
    else if (strcmp(cmd, "dm:") == 0)
    {
        if (parsed != 2)
        {
            if (write(client->fd, "Usage: /dm: <username>\r\n", 24) == -1)
            {
                perror("write");
                return -1;
            }
            return 0;
        }
        if (join_dm(arg, clients, client) == -1)
        {
            fprintf(stderr, "join_dm");
            return -1;
        }
    }
    else if (strcmp(cmd, "leave") == 0)
    {
        client->channel = -1;
        client->dm_target = NULL;

        snprintf(msg, MAX_MSG_BUF, "Left chat.\r\n");
        if (write(client->fd, msg, strlen(msg)) == -1)
        {
            perror("write");
            return -1;
        }
    }
    else if (strcmp(cmd, "who") == 0)
    {
        // case where client isnt in a channel or dm
        if (client->channel == -1)
        {
            snprintf(msg, MAX_MSG_BUF, "You are not in a channel. Use /join <name> to join one.\r\n");
            if (write(client->fd, msg, strlen(msg)) == -1)
            {
                perror("write");
                return -1;
            }
            return 0;
        }

        snprintf(msg, MAX_MSG_BUF, "Users in channel %s:\r\n", channel_arr[client->channel].name);
        if (write(client->fd, msg, strlen(msg)) == -1)
        {
            perror("write");
            return -1;
        }
        // loop thru all clients and list those in the same channel as client
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].fd != -1 && clients[i].channel == client->channel)
            {
                if (clients[i].fd == client->fd)
                {
                    snprintf(msg, MAX_MSG_BUF, " - %s (you)\r\n", clients[i].username);
                }
                else
                {
                    snprintf(msg, MAX_MSG_BUF, " - %s\r\n", clients[i].username);
                }
                if (write(client->fd, msg, strlen(msg)) == -1)
                {
                    perror("write");
                    return -1;
                }
            }
        }
    }
    else if (strcmp(cmd, "list") == 0)
    {
        if (list_active_channels(channel_arr, client->fd) == -1)
        {
            fprintf(stderr, "list_channels\n");
            return -1;
        }
    }
    else
    {
        if (write(client->fd, "Unknown command.\r\n", 18) == -1)
        {
            perror("write");
            return -1;
        }
    }

    return 0;
}

// helper for handle_client_message
int handle_message(char *buf, char *msg, Channel *channel_arr, Client *client, Client *clients)
{
    if (client->channel == -1 && client->dm_target == NULL)
    {
        snprintf(msg, MAX_MSG_BUF, "\nJoin a chat first. Use /join <name> to join a channel or /dm: <username> for private msgs:\r\n");
        if (write(client->fd, msg, strlen(msg)) == -1)
        {
            perror("write");
            return -1;
        }
    }
    else if (client->channel != -1)
    {
        snprintf(msg, MAX_MSG_BUF, "[#%s] %s: %s\r\n",
                 channel_arr[client->channel].name,
                 client->username, buf); // format message with channel and username

        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if (clients[j].fd != -1 && clients[j].fd != client->fd && clients[j].channel == client->channel) // only send to clients in same channel
            {
                if (write(clients[j].fd, msg, strlen(msg)) == -1)
                {
                    perror("write");
                    // don't return since this is an issue on another client's fd, keep sending to other clients
                }
            }
        }
    }
    else if (client->dm_target != NULL)
    {
        // check if target client is in the private chat with client already
        if (client->dm_target->dm_target != NULL && strcmp((client->dm_target)->dm_target->username, client->username) == 0)
        {
            snprintf(msg, MAX_MSG_BUF, "[Private Chat] %s: %s\r\n",
                     client->username, buf); // format message with name
            if (write(client->dm_target->fd, msg, strlen(msg)) == -1)
            {
                perror("write");
                // don't return since this is an issue on another client's fd, keep sending msg to remaining clients
            }
        }
        else
        {
            // notify target client that they're being poked
            snprintf(msg, MAX_MSG_BUF, "** %s sent you a private message. /dm: %s to go to the chat. **\r\n", client->username, client->username);
            if (write(client->dm_target->fd, msg, strlen(msg)) == -1)
            {
                perror("write");
                // don't return since this is an issue on another client's fd, keep sending msg to remaining clients
            }
        }
    }

    return 0;
}

int handle_client_message(int bytes_read, char *buf, Channel *channel_arr, Client *clients, Client *client)
{
    char msg[MAX_MSG_BUF];

    buf[bytes_read] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';

    if (client->username[0] == '\0')
    {
        // strip whitespace-only input
        int only_space = 1;
        for (int i = 0; buf[i] != '\0'; i++)
        {
            if (buf[i] != ' ' && buf[i] != '\t')
            {
                only_space = 0;
                break;
            }
        }

        if (buf[0] == '\0' || only_space)
        {
            if (write(client->fd, "Invalid username. Enter user name:\r\n", 36) == -1)
            {
                perror("write");
                return -1;
            }
            return 0; // leave username empty so next message retries
        }

        if (find_client(buf, clients) != NULL) 
        {
            if(write(client->fd, "Username is already taken. Enter different user name:\r\n", 55) == -1)
            {
                perror("write");
                return -1;
            }
            return 0;
        }
        
        strncpy(client->username, buf, 31);
        client->username[31] = '\0';

        snprintf(msg, MAX_MSG_BUF, "\nWelcome, %s!\nUse /join <name> to join a channel or /dm: <username> for private msgs.\n/list for active channels:\r\n", client->username);
        if (write(client->fd, msg, strlen(msg)) == -1)
        {
            perror("write");
            return -1;
        }
    }
    else // chat message
    {
        // check for command input
        if (buf[0] == '/')
        {
            if (handle_command(buf, channel_arr, clients, client) == -1)
            {
                fprintf(stderr, "handle_command\n");
                return -1;
            }
        }
        else
        { // regular message

            if (handle_message(buf, msg, channel_arr, client, clients) == -1)
            {
                fprintf(stderr, "handle_message\n");
                return -1;
            }
        }
    }

    return 0;
}
