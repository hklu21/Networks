#ifndef SERVERS_H
#define SERVERS_H

#include "../lib/../lib/uthash.h"
#include "client.h"
#include "channels.h"
#include "../lib/sds/sds.h"
#define BUFFER_SIZE 512


typedef struct irc_oper
{
    sds nick;     /* key for irc_oper hashtable */
    sds mode;     /* value for irc_oper hashtable */
    UT_hash_handle hh;
} irc_oper_t;


typedef struct context
{
    int num_connections;
    int total_connections;
    char *password;
    client_t *client_hashtable;
    nick_t *nicks_hashtable;
    channel_t *channels_hashtable;
    irc_oper_t *irc_operators_hashtable;
    pthread_mutex_t lock; 
    pthread_mutex_t channels_lock;
    pthread_mutex_t clients_lock;
    pthread_mutex_t nicks_lock;
    pthread_mutex_t operators_lock;

} server_ctx;

typedef struct worker_args
{
    int socket;
    /* We need to pass the server context to the worker thread */
    char *server_hostname;
    server_ctx *ctx;
} worker_args;

int server(char *port, char *passwd, char *host, char *network_file);
void close_socket (server_ctx *ctx, int client_socket);
#endif