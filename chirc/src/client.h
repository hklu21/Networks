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
    sds nick;
    sds username;
    sds realname;
    conn_status state; 
    bool is_irc_operator;
} user_t;

/* This struct is a hashtable whose key is the client's hostname
 * and value is the user_info_t struct belonging to that client
 * so that we can uniquely identify each client and their registered info
 */
typedef struct client_t
{
    int socket;  /* key is aggregate of this field */
    user_t info;  /* value for hashtable */
    UT_hash_handle hh; 
} client_t;

/* This struct is used to store the used Nickname */
typedef struct nick_t
{
    sds nick;     /* key for hashtable */
    int client_socket; /* value (key for client_t) */
    UT_hash_handle hh;
} nick_t;

nick_t * find_NICK(sds nickname, nick_t **nicks);
nick_t * add_NICK(sds nickname, int client_socket, nick_t **nicks);
client_t *find_USER(int client_socket, client_t **client_hashtable);
client_t * add_USER(client_t *client, int client_socket, client_t **client_hashtable);
void remove_NICK(sds nick, nick_t **nicks);
void remove_USER(int client_socket, client_t **clients);
#endif