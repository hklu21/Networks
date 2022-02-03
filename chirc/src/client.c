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
#include "client.h"
#include "log.h"
#include "../lib/uthash.h"

nick_t * find_NICK(sds nickname, nick_t **nicks) {
    nick_t *nickvalue;
    HASH_FIND_STR(*nicks, nickname, nickvalue);
    return nickvalue;
}

nick_t * add_NICK(sds nickname, int client_socket, nick_t **nicks) {
    nick_t *nickvalue;
    HASH_FIND_STR(*nicks, nickname, nickvalue);
    if ( nickvalue!= NULL) {
        return nickvalue;
    }
   
    nick_t *nick_add = (nick_t *)malloc(sizeof(nick_t));
    nick_add->nick = sdsempty();
    nick_add->nick = sdscpy(nick_add->nick, nickname);
    nick_add->client_socket = client_socket;
    HASH_ADD_STR(*nicks, nick, nick_add);
    //chilog(INFO, "Add nick %s\n", nickname);
    return nick_add;
}

client_t *find_USER(int client_socket, client_t **client_hashtable) {
    client_t *uservalue;
    HASH_FIND_INT(*client_hashtable, &client_socket, uservalue);
    return uservalue;
}


client_t * add_USER(client_t *client, int client_socket, client_t **client_hashtable){
    client_t *value;
    HASH_FIND_INT(*client_hashtable, &client_socket, value);
    if ( value!= NULL) {
        return value;
    }
    HASH_ADD_INT(*client_hashtable, socket, client);
    chilog(INFO, "client_hashtable size %d\n", HASH_COUNT(*client_hashtable));
    return client;
}

void remove_NICK(sds nick, nick_t **nicks)
{
    /* Remove nick entry from nicks hash table with given nick */
    nick_t *nick_to_remove;
    HASH_FIND_STR(*nicks, nick, nick_to_remove);
    if (nick_to_remove != NULL)
    {
        HASH_DELETE(hh, *nicks, nick_to_remove);
        free(nick_to_remove);
    }
}

void remove_USER(int client_socket, client_t **clients)
{
    /* Remove user entry from clients hash table with given socket */
    client_t *client;
    HASH_FIND_INT(*clients, &client_socket, client);
    if (client != NULL)
    {
        HASH_DELETE(hh, *clients, client);
        free(client);
    }
}