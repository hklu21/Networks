#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
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
#include "handlers.h"
#include <pthread.h>
#include "../lib/sds/sds.h"
#include "../lib/uthash.h"
#include "server.h"
#include "server_cmd.h"
#include "log.h"
#include "reply.h"

/*
 * service_single_client - single worker thread function
 *
 * args: worker arguments
 *
 * Return: nothing
 */
void *service_single_client(void *args);


/*
 * free_ctx - free context and all its allocated memory
 *
 * ctx: server context
 *
 * Return: nothing
 */
void free_ctx(server_ctx *ctx);


int server(char *port, char *passwd, char *servername, char *network_file)
{
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
    /* Initialize context */
    server_ctx *ctx = calloc(1, sizeof(server_ctx));
    ctx->num_connected_users = 0;                   /* Number of connected clients, used in LUSERS */
    ctx->total_connections = 0;                     /* Number of total connections, used in LUSERS */
    ctx->password = passwd;                         /* User password, read from input */
    ctx->client_hashtable = NULL;                   /* Client_hashtable to store all connections */
    ctx->nicks_hashtable = NULL;                    /* Nicks_hashtable to store all user nicknames */
    ctx->channels_hashtable = NULL;                 /* Channels_hashtable to store all channels */
    ctx->irc_operators_hashtable = NULL;            /* IRC_operator_hashtable to store all operators */
    pthread_mutex_init(&ctx->lock, NULL);           /* Initiate lock to protect num_connection and total_connections */
    pthread_mutex_init(&ctx->channels_lock, NULL);  /* Initiate lock to protect channels hashtable */
    pthread_mutex_init(&ctx->clients_lock, NULL);   /* Initiate lock to protect clients hashtable */
    pthread_mutex_init(&ctx->nicks_lock, NULL);     /* Initiate lock to protect nicks hashtable */
    pthread_mutex_init(&ctx->operators_lock, NULL); /* Initiate lock to protect operators hashtable */
    pthread_mutex_init(&ctx->socket_lock, NULL);    /* Initiate lock to protect sendall */

    sigset_t new;
    sigemptyset(&new);
    sigaddset(&new, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &new, NULL) != 0)
    {
        perror("Unable to mask SIGPIPE");
        exit(-1);
    }
    int server_socket;
    int client_socket;
    struct addrinfo hints, *res, *p;
    struct sockaddr_storage *client_addr = NULL;
    int yes = 1;
    socklen_t sin_size = sizeof(struct sockaddr_in);
    pthread_t worker_thread;
    worker_args(*wa);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // Return my address, so I can bind() to it

    /* Call getaddrinfo with the host parameter set to NULL */
    if (getaddrinfo(NULL, port, &hints, &res) != 0)
    {
        perror("getaddrinfo() failed");
        pthread_exit(NULL);
    }

    for (p = res; p != NULL; p = p->ai_next)
    {
        if ((server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("Could not open socket");
            continue;
        }

        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("Socket setsockopt() failed");
            close(server_socket);
            continue;
        }

        if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("Socket bind() failed");
            close(server_socket);
            continue;
        }

        if (listen(server_socket, 5) == -1)
        {
            perror("Socket listen() failed");
            close(server_socket);
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (p == NULL)
    {
        chilog(ERROR, "Could not find a socket to bind to.\n");
        pthread_exit(NULL);
    }

    while (1)
    {
        client_addr = calloc(1, sin_size);
        if ((client_socket = accept(server_socket, (struct sockaddr *)client_addr, &sin_size)) == -1)
        {
            free(client_addr);
            chilog(ERROR, "Could not accept() connection");
            continue;
        }

        char client_hostname[MAX_STR_LEN];
        char port[100];
        int result = getnameinfo((struct sockaddr *)client_addr,
                                 sin_size,
                                 client_hostname,
                                 sizeof client_hostname,
                                 port,
                                 sizeof port, 0);
        wa = calloc(1, sizeof(worker_args));
        wa->socket = client_socket;
        wa->client_hostname = sdsnew(client_hostname);
        wa->ctx = ctx;

        if (pthread_create(&worker_thread, NULL, service_single_client, wa) != 0)
        {
            perror("Could not create a worker thread");
            free(client_addr);
            free(wa);
            close(client_socket);
            close(server_socket);
            return EXIT_FAILURE;
        }
    }

    pthread_mutex_destroy(&ctx->nicks_lock);
    pthread_mutex_destroy(&ctx->channels_lock);
    pthread_mutex_destroy(&ctx->clients_lock);
    pthread_mutex_destroy(&ctx->operators_lock);
    pthread_mutex_destroy(&ctx->socket_lock);
    pthread_mutex_destroy(&ctx->lock);

    free_ctx(ctx);

    return EXIT_SUCCESS;
}


void *service_single_client(void *args)
{
    /*
     * service_single_client - single worker thread function
     *
     * args: worker arguments
     *
     * Return: nothing
     */
    worker_args *wa;
    server_ctx *ctx;
    int client_socket = 0;                  // Client_socket
    sds server_hostname = sdsempty();       // Server_hostname, prefix of msg
    sds client_hostname;                    // Client_hostname, prefix of msg
    int nbytes = 0;                         // Length of command from the client
    int count = 0;                          // Length of tokens
    char buffer[BUFFER_SIZE];               // Command received from the client
    sds cmdstack = sdsempty();              // Received but untreated command

    wa = (struct worker_args *)args;
    client_socket = wa->socket;
    ctx = wa->ctx;
    client_hostname = wa->client_hostname;

    add_total_connected_number(ctx);

    /* Get server host name */
    char server_host[MAX_STR_LEN];
    int host_check = gethostname(server_host, sizeof server_host);
    if (host_check == -1) 
    {
        chilog(ERROR, "gethostname() failed");
        exit(CHIRC_ERROR);
    }
    server_hostname = sdsnew(server_host);

    /* Initialize connection struc with client socket,
     * server hostname and client hostname */
    conn_info_t *conn = calloc(1, sizeof(conn_info_t));
    conn->client_socket = client_socket;
    conn->server_hostname = server_hostname;
    conn->client_hostname = client_hostname;

    pthread_detach(pthread_self());

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        nbytes = recv(client_socket, buffer, sizeof(buffer), 0);

        /* a return code of 0 from recv means that the client has disconnected; */
        /* a return code of -1 is errors */
        if (nbytes <= 0)
        {
            sdsfree(conn->server_hostname);
            sdsfree(conn->client_hostname);
            free(conn);
            
            close_socket(ctx, client_socket);
            pthread_exit(NULL);
        }
        // Add NULL terminator to manipulate the bytes returned by recv() as a C-string
        buffer[nbytes] = '\0';
        cmdstack = sdscat(cmdstack, buffer);

        /* Design: a cmd stack for assembling the next message that will be processed.
         * Split the untreated command information into whole command segments if possible. */
        sds *cmdseg; // Command segments

        cmdseg = sdssplitlen(cmdstack, sdslen(cmdstack), "\r\n", 2, &count);
        if (count <= 1) // Not meet the end of a command
        {
            sdsfreesplitres(cmdseg, count);
            continue;
        }
        else
        {
            /* Have (count - 1) whole commands */
            int i, argc;
            sds *cmdtokens;
            for (i = 0; i < count - 1; i++)
            {
                sdstrim(cmdseg[i], " ");
                cmdtokens = sdssplitlen(cmdseg[i], sdslen(cmdseg[i]), " ", 1, &argc);
                handle_request(ctx, cmdtokens, argc, conn);
                sdsfreesplitres(cmdtokens, argc);
            }
            sdsrange(cmdstack, (int)sdslen(cmdstack) - (int)sdslen(cmdseg[count - 1]), (int)sdslen(cmdstack));
            sdsfreesplitres(cmdseg, count);
        }
    }
}


void free_ctx(server_ctx *ctx)
{
    /*
     * free_ctx - free context and all its allocated memory
     *
     * ctx: server context
     *
     * Return: nothing
     */
    client_t *client_ht, *client_tmp;
    nick_t *nicks_ht, *nick_tmp;
    channel_t *channels_ht, *channel_tmp;
    irc_oper_t *irc_operators_ht, *irc_temp;
    HASH_ITER(hh, ctx->client_hashtable, client_ht, client_tmp)
    {
        HASH_DEL(ctx->client_hashtable, client_ht);
        free(client_ht); /* free it */
    }
    HASH_ITER(hh, ctx->nicks_hashtable, nicks_ht, nick_tmp)
    {
        HASH_DEL(ctx->nicks_hashtable, nicks_ht);
        free(nicks_ht); /* free it */
    }
    HASH_ITER(hh, ctx->channels_hashtable, channels_ht, channel_tmp)
    {
        HASH_DEL(ctx->channels_hashtable, channels_ht);
        free(channels_ht); /* free it */
    }
    HASH_ITER(hh, ctx->irc_operators_hashtable, irc_operators_ht, irc_temp)
    {
        HASH_DEL(ctx->irc_operators_hashtable, irc_operators_ht);
        free(irc_operators_ht); /* free it */
    }
    free(ctx);
}


void close_socket(server_ctx *ctx, int client_socket)
{
    /*
     * close_socket - Close socket when exit
     *
     * ctx: server context
     *
     * client_socket: the socket to be closed
     *
     * Return: nothing
     */
    pthread_mutex_lock(&ctx->socket_lock);
    close(client_socket);
    pthread_mutex_unlock(&ctx->socket_lock);
}
