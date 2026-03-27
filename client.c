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
int remove_client(Client *client)
{
    printf("%s disconected\n", client->username);

    if (close(client->fd) == -1)
    {
        perror("close");
        return -1;
    }

    client->fd = -1;
    client->username[0] = '\0';

    return 0;
}

// helper for handle_client_message
int handle_command(char *buf, Channel *channel_arr, Client *client)
{
    char msg[MAX_MSG_BUF];
    char cmd[32], arg[64];
    int parsed = sscanf(buf + 1, "%31s %63s", cmd, arg);

    if (strcmp(cmd, "join") == 0 && parsed == 2)
    {
        // remove CR/LF from arg
        arg[strcspn(arg, "\r\n")] = '\0';

        int ch = find_or_create_channel(arg, channel_arr);
        if (ch == -1)
        {
            write(client->fd, "Error: too many channels.\r\n", 26);
            return -1;
        }

        client->channel = ch;
        snprintf(msg, sizeof(msg), "----Joined #%s----\n(You can't see previous messages)\nChat:\r\n", arg);

        if (write(client->fd, msg, strlen(msg)) == -1)
        {
            perror("write");
            // close fd?
            return -1;
        }
    }
    else if (strcmp(cmd, "leave") == 0)
    {
        client->channel = -1;

        if (write(client->fd, "Left channel.\r\n", 15) == -1)
        {
            perror("write");
            // close fd?
            return -1;
        }
    }
    else
    {
        if (write(client->fd, "Unknown command.\r\n", 18) == -1)
        {
            perror("write");
            // close fd?
            return -1;
        }
    }

    return 0;
}

// helper for handle_client_message
int handle_message(char *buf, char *msg, Channel *channel_arr, Client *client, Client *clients)
{
    if (client->channel == -1)
    {
        write(client->fd, "\nJoin a channel first. Use /join <name>:\r\n", 42);
    }
    else
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
                    // close fd?
                    return -1;
                }
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

        strncpy(client->username, buf, 31);
        client->username[31] = '\0';

        snprintf(msg, MAX_BUF, "\nWelcome, %s!\nBefore you chat, join/create a channel by typing /join <channel name>:\r\n", client->username);
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
            if (handle_command(buf, channel_arr, client) == -1)
            {
                // close fd?
                fprintf(stderr, "handle_command\n");
                return -1;
            }
        }
        else
        { // regular message - broadcast to channel
            if (handle_message(buf, msg, channel_arr, client, clients) == -1)
            {
                // close fd?
                fprintf(stderr, "handle_message\n");
                return -1;
            }
        }
    }

    return 0;
}