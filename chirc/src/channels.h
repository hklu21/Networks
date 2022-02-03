#ifndef CHANNELS_H_
#define CHANNELS_H_

#include <pthread.h>
#include <stdbool.h>

#include "../lib/uthash.h"
#include "../lib/sds/sds.h"

/* A hash table for clients' information in a channel */
typedef struct channel_client
{
    sds nick;       /* key for hashtable */
    sds mode;   
    UT_hash_handle hh;
} channel_client;

typedef struct channel_t
{
    sds channel_name;  /* key for hashtable */
    channel_client *channel_clients;
    UT_hash_handle hh;
    pthread_mutex_t lock;
} channel_t;

channel_t * find_CHANNEL(sds channelname, channel_t **channels);
channel_t * add_CHANNEL(sds channelname, channel_t **channels);

channel_client * find_CHANNEL_CLIENT(sds nickname, channel_client **channel_clients);
channel_client * add_CHANNEL_CLIENT(sds nickname, channel_client **channel_clients);
void remove_CHANNEL(sds channelname,  channel_t **channels);
void remove_CHANNEL_CLIENT(sds nickname,  channel_client **channel_clients);

#endif