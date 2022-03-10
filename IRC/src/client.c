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


nick_t *find_NICK(sds nickname, nick_t **nicks)
{
    /*
     * find_NICK -  Find nickname from nickname table if exists(Not thread-safe)
     *
     * nickname: The nickname you want to search as key
     *
     * nicks: nicks hashtable which include thenickname
     *
     * Return: The searched nick_t pointer.
     */
    nick_t *nickvalue;
    HASH_FIND_STR(*nicks, nickname, nickvalue);
    return nickvalue;
}


nick_t *add_NICK(sds nickname, int client_socket, nick_t **nicks)
{
    /*
     * add_NICK -  Add nickname with the socket to the given nickname from nicks hashtable if not
     * exists; If exists, it will return the searched value (Not thread-safe)
     *
     * nickname: The nickname you want to insert as key
     *
     * client_socket: The client_socket associated with the nickname of the user.
     *
     * nicks: Channel_client hashtable which include the channel_client
     *
     * Return: The added nick_t pointer.
     */
    nick_t *nickvalue;
    HASH_FIND_STR(*nicks, nickname, nickvalue);

    if (nickvalue != NULL)
    {
        return nickvalue;
    }

    nick_t *nick_add = (nick_t *)malloc(sizeof(nick_t));
    nick_add->nick = sdsempty();
    nick_add->nick = sdscpy(nick_add->nick, nickname);
    nick_add->client_socket = client_socket;
    HASH_ADD_STR(*nicks, nick, nick_add);
    return nick_add;
}


client_t *find_USER(int client_socket, client_t **client_hashtable)
{
    /*
     * find_USER -  Find connected client from client_hashtable table if exists(Not thread-safe)
     *
     * client_socket: The client_socket you want to search as key
     *
     * client_hashtable: hashtable which include the user
     *
     * Return: The searched client_t pointer.
     */
    client_t *uservalue;
    HASH_FIND_INT(*client_hashtable, &client_socket, uservalue);
    return uservalue;
}


client_t *add_USER(client_t *client, int client_socket, client_t **client_hashtable)
{

    /*
     * add_USER -  Add client to client_hashtable table (Not thread-safe)
     *
     * client: The client to be added
     *
     * client_socket: The client socket for the client
     *
     * client_hashtable: hashtable which include the user
     *
     * Return: The added client_t pointer.
     */
    client_t *value;
    HASH_FIND_INT(*client_hashtable, &client_socket, value);

    if (value != NULL)
    {
        return value;
    }

    HASH_ADD_INT(*client_hashtable, socket, client);
    return client;
}


void remove_NICK(sds nick, nick_t **nicks)
{
    /*
     * remove_NICK -  Remove given nickname from nicks client_hashtable table if exists(Not thread-safe)
     *
     * nick: The nickname you want to remove as key
     *
     * nicks: hashtable which include the nickname
     * 
     * Return: nothing
     */
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

    /*
     * remove_USER -  Remove user with given client_socket from clients table if exists(Not thread-safe)
     *
     * client_socket: The client_socket you want to remove as key
     *
     * clients: hashtable which include the client_socket
     * 
     * Return: nothing
     */
    client_t *client;
    HASH_FIND_INT(*clients, &client_socket, client);
    
    if (client != NULL)
    {
        HASH_DELETE(hh, *clients, client);
        free(client);
    }
}