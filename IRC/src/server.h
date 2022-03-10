#ifndef SERVERS_H
#define SERVERS_H

#include "../lib/../lib/uthash.h"
#include "client.h"
#include "channels.h"
#include "../lib/sds/sds.h"
#define BUFFER_SIZE 512
#define MAX_STR_LEN 100

typedef struct irc_oper
{
    sds nick; /* Key for irc_oper hashtable */
    sds mode; /* Value for irc_oper hashtable */
    UT_hash_handle hh;
} irc_oper_t;

/* Context is a struct that contains server shared information
 * amongst all the worker threads */
typedef struct context
{
    int num_connected_users;             /* Number of user connections */
    int total_connections;               /* Total number of user & server connections */
    char *password;                      /* User Password */
    client_t *client_hashtable;          /* User connection hashtable */
    nick_t *nicks_hashtable;             /* Nicks hashtable */
    channel_t *channels_hashtable;       /* Channels hashtable */
    irc_oper_t *irc_operators_hashtable; /* Irc_operators hashtable */
    pthread_mutex_t lock;                /* Locks to protect number_connections and total_connections */
    pthread_mutex_t channels_lock;       /* Locks to protect channels hashtable and channel_clients hashtable */
    pthread_mutex_t clients_lock;        /* Locks to protect clients hashtable */
    pthread_mutex_t nicks_lock;          /* Locks to protect nicks hashtable */
    pthread_mutex_t operators_lock;      /* Locks to protect irc_operators hashtable */
    pthread_mutex_t socket_lock;         /* Locks to protect sendall() function */

} server_ctx;

/* Worker_args struct is local to worker thread to hold server context info */
typedef struct worker_args
{
    int socket;          /* Server socket */
    sds client_hostname; /* Client hostname, e.g. "foo.example.com" */
    server_ctx *ctx;     /* Server context pointer */
} worker_args;

typedef struct conn_info
{
    int client_socket;   /* Client socket */
    sds server_hostname; /* Server hostname, e.g. "bar.example.com" */
    sds client_hostname; /* Client hostname, e.g. "foo.example.com" */
} conn_info_t;

/*
 * server - Initialize server context and handle multi-clients
 *
 * port: port number
 *
 * passwd: operator password specified in main.c
 *
 * host: server_hostname
 *
 * network_file: network file specified in main.c
 *
 * Return: EXIT_SUCCESS/EXIT_FAILURE
 *
 */
int server(char *port, char *passwd, char *host, char *network_file);

/*
 * close_socket - Close socket when exit
 *
 * ctx: server context
 *
 * client_socket: the socket to be closed
 *
 * Return: nothing
 */
void close_socket(server_ctx *ctx, int client_socket);

#endif