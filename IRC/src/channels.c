#include "../lib/sds/sds.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "log.h"
#include "channels.h"
#include "../lib/uthash.h"


channel_t *find_CHANNEL(sds channelname, channel_t **channels)
{
    /*
     * find_CHANNEL -  Find channel with the given channel name (Not thread-safe)
     *
     * channelname: The channel name you want to search as key
     *
     * channels: Channels hashtable which include channels
     *
     * Returns: The channel with given name or NULL if not exists.
     */
    channel_t *channelvalue = NULL;
    HASH_FIND_STR(*channels, channelname, channelvalue);
    return channelvalue;
}


channel_t *add_CHANNEL(sds channelname, channel_t **channels)
{
    /*
     * add_CHANNEL -  Add channel with the given channel name (Not thread-safe)
     *
     * channelname: The channel name you want to insert as key
     *
     * channels: Channels hashtable which include channels
     *
     * Returns: The channel with given name after adding it to the hashtable.
     */
    channel_t *channelvalue = NULL;
    HASH_FIND_STR(*channels, channelname, channelvalue);

    if (channelvalue != NULL)
    {
        return channelvalue;
    }

    channel_t *channel_add = malloc(sizeof(channel_t));
    channel_add->channel_clients = NULL;
    channel_add->channel_name = sdsempty();
    channel_add->channel_name = sdscpy(channel_add->channel_name, channelname);

    HASH_ADD_STR(*channels, channel_name, channel_add);
    return channel_add;
}


channel_client *find_CHANNEL_CLIENT(sds nickname, channel_client **channel_clients)
{
    /*
     * find_CHANNEL_CLIENT -  Find the client in channel with the given nick name (Not thread-safe)
     *
     * nickname: The nickname of the client you want to search as key
     *
     * channel_clients: Channel_client hashtable which will include the client
     *
     * Returns: The channel_client with given nickname.
     */
    channel_client *clientvalue = NULL;
    HASH_FIND_STR(*channel_clients, nickname, clientvalue);

    return clientvalue;
}


channel_client *add_CHANNEL_CLIENT(sds nickname, channel_client **channel_clients)
{
    /*
     * add_CHANNEL_CLIENT -  Add client with the given channel name to the channel(Not thread-safe)
     *
     * nickname: The nickname you want to insert the client into the channel as key
     *
     * channel_clients: Channel_client hashtable which will include the client
     *
     * Returns: The channel_client with given nickname after adding it to the hashtable.
     */
    channel_client *clientvalue = NULL;
    HASH_FIND_STR(*channel_clients, nickname, clientvalue);

    if (clientvalue != NULL)
    {
        return clientvalue;
    }
    channel_client *client_add = malloc(sizeof(channel_client));
    client_add->nick = sdsempty();
    client_add->nick = sdscpy(client_add->nick, nickname);
    HASH_ADD_STR(*channel_clients, nick, client_add);

    return client_add;
}

void remove_CHANNEL(sds channelname, channel_t **channels)
{
    /*
     * remove_CHANNEL -  Remove channel with the given channelname (Not thread-safe)
     *
     * channelname: The channel name you want to remove as key
     *
     * channels: Channels hashtable which include channels
     * 
     * Return: nothing
     */
    channel_t *channel_to_remove;
    HASH_FIND_STR(*channels, channelname, channel_to_remove);

    if (channel_to_remove != NULL)
    {
        HASH_DELETE(hh, *channels, channel_to_remove);
        free(channel_to_remove);
    }
}


void remove_CHANNEL_CLIENT(sds nickname, channel_client **channel_clients)
{
    /*
     * remove_CHANNEL_CLIENT -  Remove channel_client with the given nickname from channels(Not thread-safe)
     *
     * nickname: The nickname you want to remove as key
     *
     * channel_clients: Channel_client hashtable which include the channel_client
     *
     * Return: nothing
     */
    channel_client *client_to_remove;
    HASH_FIND_STR(*channel_clients, nickname, client_to_remove);
    
    if (client_to_remove != NULL)
    {
        HASH_DELETE(hh, *channel_clients, client_to_remove);
        free(client_to_remove);
    }
}