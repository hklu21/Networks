#ifndef CHANNELS_H_
#define CHANNELS_H_

#include <pthread.h>
#include <stdbool.h>
#include "../lib/uthash.h"
#include "../lib/sds/sds.h"


/* A hash table for clients' information in a channel */
typedef struct channel_client
{
    /* Key for hashtable */
    sds nick;
    /* 
     * The mode for the client,
     * "o" if first added, could be
     * "+o" or "-o" in different modes 
     */
    sds mode;
    UT_hash_handle hh;
} channel_client;


typedef struct channel_t
{
    /* key for hashtable */
    sds channel_name;
    /* channel_clients hashtable for the channel*/
    channel_client *channel_clients;
    UT_hash_handle hh;
} channel_t;


/*
 * find_CHANNEL -  Find channel with the given channel name (Not thread-safe)
 *
 * channelname: The channel name you want to search as key
 *
 * channels: Channels hashtable which include channels
 *
 * Returns: The channel with given name or NULL if not exists.
 */
channel_t *find_CHANNEL(sds channelname, channel_t **channels);


/*
 * add_CHANNEL -  Add channel with the given channel name (Not thread-safe)
 *
 * channelname: The channel name you want to insert as key
 *
 * channels: Channels hashtable which include channels
 *
 * Returns: The channel with given name after adding it to the hashtable.
 */
channel_t *add_CHANNEL(sds channelname, channel_t **channels);


/*
 * find_CHANNEL_CLIENT -  Find the client in channel with the given nick name (Not thread-safe)
 *
 * nickname: The nickname of the client you want to search as key
 *
 * channel_clients: Channel_client hashtable which will include the client
 *
 * Returns: The channel_client with given nickname.
 */
channel_client *find_CHANNEL_CLIENT(sds nickname, channel_client **channel_clients);


/*
 * add_CHANNEL_CLIENT -  Add client with the given channel name to the channel(Not thread-safe)
 *
 * nickname: The nickname you want to insert the client into the channel as key
 *
 * channel_clients: Channel_client hashtable which will include the client
 *
 * Returns: The channel_client with given nickname after adding it to the hashtable.
 */
channel_client *add_CHANNEL_CLIENT(sds nickname, channel_client **channel_clients);


/*
 * remove_CHANNEL -  Remove channel with the given channelname (Not thread-safe)
 *
 * channelname: The channel name you want to remove as key
 *
 * channels: Channels hashtable which include channels
 * 
 * Return: nothing
 */
void remove_CHANNEL(sds channelname, channel_t **channels);


/*
 * remove_CHANNEL_CLIENT -  Remove channel_client with the given nickname from channels(Not thread-safe)
 *
 * nickname: The nickname you want to remove as key
 *
 * channel_clients: Channel_client hashtable which include the channel_client
 *
 * Returns: nothing
 */
void remove_CHANNEL_CLIENT(sds nickname, channel_client **channel_clients);

#endif