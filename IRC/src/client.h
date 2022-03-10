#ifndef CLIENTS_H_
#define CLIENTS_H_

#include <pthread.h>
#include <stdbool.h>
#include "../lib/uthash.h"
#include "../lib/sds/sds.h"

typedef enum
{
    NOT_REGISTERED = 0,
    USER_MISSING = 1,
    NICK_MISSING = 2,
    REGISTERED = 3
} conn_status;

/* user_info is used to record user information upon registration */
typedef struct user_info
{
    sds nick;             // User's nickname
    sds username;         // User's username
    sds realname;         // User's realname
    conn_status state;    // User's connected status, defined in conn_status struct
    bool is_irc_operator; // If the user is irc_operator or channel operator
} user_t;

/* This struct is a hashtable whose key is the client's hostname
 * and value is the user_info_t struct belonging to that client
 * so that we can uniquely identify each client and their registered info
 */
typedef struct client_t
{
    int socket;          /* key for hastable */
    sds client_hostname; /* client hostname */
    user_t info;         /* value for hashtable */
    UT_hash_handle hh;
} client_t;

/* This struct is used to store the used Nickname */
typedef struct nick_t
{
    sds nick;          /* key for hashtable */
    int client_socket; /* value (key for client_t) */
    UT_hash_handle hh;
} nick_t;

/*
 * find_NICK -  Find nickname from nickname table if exists(Not thread-safe)
 *
 * nickname: The nickname you want to search as key
 *
 * nicks: nicks hashtable which include the nickname
 *
 * Return: The searched nick_t pointer.
 */
nick_t *find_NICK(sds nickname, nick_t **nicks);

/*
 * add_NICK -  Add nickname with the socket to the given nickname
 * from nicks hashtable if not exists; If exists, it will
 * return the searched value (Not thread-safe)
 *
 * nickname: The nickname you want to insert as key
 *
 * client_socket: The client_socket associated with the nickname of the user.
 *
 * nicks: Channel_client hashtable which include the channel_client
 *
 * Return: The added nick_t pointer.
 */
nick_t *add_NICK(sds nickname, int client_socket, nick_t **nicks);

/*
 * find_USER -  Find connected client from client_hashtable
 * table if exists(Not thread-safe)
 *
 * client_socket: The client_socket you want to search as key
 *
 * client_hashtable: hashtable which include the user
 *
 * Return: The searched client_t pointer.
 */
client_t *find_USER(int client_socket, client_t **client_hashtable);

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
client_t *add_USER(client_t *client, int client_socket,
                   client_t **client_hashtable);

/*
 * remove_NICK -  Remove given nickname from nicks
 * hashtable table if exists(Not thread-safe)
 *
 * nick: The nickname you want to remove as key
 *
 * nicks: hashtable which include the nickname
 *
 * Returns: nothing
 */
void remove_NICK(sds nick, nick_t **nicks);

/*
 * remove_USER -  Remove user with given client_socket from clients 
 * table if exists(Not thread-safe)
 *
 * client_socket: The client_socket you want to remove as key
 *
 * clients: hashtable which include the client_socket
 *
 * Returns: nothing
 */
void remove_USER(int client_socket, client_t **clients);
#endif