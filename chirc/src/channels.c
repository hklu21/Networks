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



channel_t * find_CHANNEL(sds channelname, channel_t **channels) {
    channel_t *channelvalue;
    HASH_FIND_STR(*channels, channelname, channelvalue);
    return channelvalue;
}

channel_t * add_CHANNEL(sds channelname, channel_t **channels) {
    channel_t *channelvalue;
    HASH_FIND_STR(*channels, channelname, channelvalue);
    if ( channelvalue!= NULL) {
        return channelvalue;
    }
   
    channel_t *channel_add = malloc(sizeof(channel_t));
    channel_add->channel_clients = NULL;
    channel_add->channel_name = sdsempty();
    channel_add->channel_name = sdscpy(channel_add->channel_name, channelname);
    HASH_ADD_STR(*channels, channel_name, channel_add);
    chilog(INFO, "channel_hashtable size %d\n", HASH_COUNT(*channels));
    return channel_add;
}

channel_client * find_CHANNEL_CLIENT(sds nickname, channel_client **channel_clients){
    channel_client *clientvalue;
    HASH_FIND_STR(*channel_clients, nickname, clientvalue);
    return clientvalue;
}

channel_client * add_CHANNEL_CLIENT(sds nickname, channel_client **channel_clients){
    channel_client *clientvalue;
    HASH_FIND_STR(*channel_clients, nickname, clientvalue);
    if ( clientvalue != NULL) {
        return clientvalue;
    }
    channel_client *client_add = malloc(sizeof(channel_client));
    client_add->nick = sdsempty();
    client_add->nick = sdscpy(client_add->nick, nickname);
    HASH_ADD_STR(*channel_clients, nick, client_add);
    chilog(INFO, "channel_clients_hashtable size %d\n", HASH_COUNT(*channel_clients));
    return client_add;
}

void remove_CHANNEL(sds channelname,  channel_t **channels)
{
    channel_t *channel_to_remove;
    HASH_FIND_STR(*channels, channelname, channel_to_remove);
    if (channel_to_remove != NULL)
    {
        HASH_DELETE(hh, *channels, channel_to_remove);
        free(channel_to_remove);
    }
}

void remove_CHANNEL_CLIENT(sds nickname,  channel_client **channel_clients)
{
    channel_client *client_to_remove;
    HASH_FIND_STR(*channel_clients, nickname, client_to_remove);
    if (client_to_remove != NULL)
    {
        HASH_DELETE(hh, *channel_clients, client_to_remove);
        free(client_to_remove);
    }
}