#ifndef CHANNEL_H
#define CHANNEL_H
#define MAX_CHANNEL_NAME 32

typedef struct Channel
{
    char name[MAX_CHANNEL_NAME];
    int active; // 1 if channel is active, 0 if slot is empty
} Channel;

int find_or_create_channel(const char *name, Channel *channels);

int list_active_channels(Channel *channels, int client_fd);

#endif
