#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "channel.h"
#include "config.h"

// returns index of channel, or -1 if full
int find_or_create_channel(const char *name, Channel *channels)
{
    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        if (channels[i].active && strcmp(channels[i].name, name) == 0) // found existing
            return i;
    }
    // create new channel
    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        if (!channels[i].active)
        {
            strncpy(channels[i].name, name, MAX_CHANNEL_NAME - 1);
            channels[i].active = 1;
            return i;
        }
    }
    return -1; // full
}


int list_active_channels(Channel *channels, int client_fd)
{
    char buf[MAX_BUF];
    int offset = 0;

    offset += snprintf(buf + offset, sizeof(buf) - offset, "Active channels:\n");

    // append each active channel name to buffer
    for (int i = 0; i < MAX_CHANNELS; i++)
    {
        if (channels[i].active)
            offset += snprintf(buf + offset, sizeof(buf) - offset, "  - %s\n", channels[i].name);
    }

    // send the full buffer to the client
    if (write(client_fd, buf, offset) == -1)
    {
        perror("write");
        return -1;
    }
    return 0;
}
