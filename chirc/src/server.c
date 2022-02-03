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
#include "server.h"
#include "log.h"
#include "reply.h"

void *service_single_client(void *args);

int server(char *port, char *passwd, char *servername, char *network_file)
{
    /* Initialize context */
    server_ctx *ctx = calloc(1, sizeof(server_ctx));
    ctx->num_connections = 0;
    ctx->total_connections = 0;
    ctx->password = passwd;
    ctx->client_hashtable = NULL;
    ctx->nicks_hashtable = NULL;
    ctx->channels_hashtable = NULL;
    ctx->irc_operators_hashtable = NULL;
    pthread_mutex_init(&ctx->lock, NULL);
    pthread_mutex_init(&ctx->channels_lock, NULL);
    pthread_mutex_init(&ctx->clients_lock, NULL);
    pthread_mutex_init(&ctx->nicks_lock, NULL);
    pthread_mutex_init(&ctx->operators_lock, NULL);


    sigset_t new;
    sigemptyset (&new);
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
    worker_args (*wa);
    

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // Return my address, so I can bind() to it
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

    int ret = 0;

    while (1)
    {
        client_addr = calloc(1, sin_size);
        if ((client_socket = accept(server_socket, (struct sockaddr *)client_addr, &sin_size)) == -1)
        {
            free(client_addr);
            chilog(ERROR, "Could not accept() connection");
            continue;
        }

        char client_hostname[100];
        char port[100];
        int result = getnameinfo((struct sockaddr *) client_addr,
                                    sin_size, 
                                    client_hostname,
                                    sizeof client_hostname,
                                    port,
                                    sizeof port, 0);
        wa = calloc(1, sizeof(worker_args));
        wa->socket = client_socket;
        wa->server_hostname = ":bar.example.com";
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
    pthread_mutex_destroy(&ctx->lock);
    free(ctx->irc_operators_hashtable);
    free(ctx);

    return EXIT_SUCCESS;
}

void *service_single_client(void *args) {
    worker_args *wa;
    server_ctx *ctx;
    int client_socket = 0;
    char *server_hostname = NULL;
    int nbytes = 0;           // length of command from the client
    int count = 0;            // length of tokens
    char buffer[BUFFER_SIZE]; // command received from the client
    sds cmdstack = sdsempty();                // received but untreated command

    wa = (struct worker_args*) args;
    client_socket = wa->socket;
    server_hostname = wa->server_hostname;
    ctx = wa->ctx;

    pthread_mutex_lock(&ctx->lock);
    ctx->total_connections++;
    pthread_mutex_unlock(&ctx->lock);

    pthread_detach(pthread_self());

    while (1)
    {
        memset(buffer, 0, sizeof(buffer));
        nbytes = recv(client_socket, buffer, sizeof(buffer), 0);

        if (nbytes <= 0)
        {
            chilog(TRACE, "Client Closed");
            //sdsfree(cmdstack);
            close_socket(ctx, client_socket);
            break;
        }

    
        cmdstack = sdscat(cmdstack, buffer);

        /* split the untreated command information into whole command segments if possible */
        sds *cmdseg; // command segments
        
        cmdseg = sdssplitlen(cmdstack, sdslen(cmdstack), "\r\n", 2, &count);
        if (count <= 1) // not meet the end of a command
        {
            sdsfreesplitres(cmdseg, count);
            continue;
        }
        else
        {
            /* have count - 1 whole commands */
            int i, argc;
            sds *cmdtokens;
            for (i = 0; i < count - 1; i++)
            {
                sdstrim(cmdseg[i]," ");
                cmdtokens = sdssplitlen(cmdseg[i], sdslen(cmdseg[i]), " ", 1, &argc);
                chilog(INFO, "%s\n", cmdseg[i]);         
                handle_request(ctx, client_socket, cmdtokens, argc, server_hostname);
                sdsfreesplitres(cmdtokens, argc);
            }
            sdsrange(cmdstack, (int)sdslen(cmdstack) - (int)sdslen(cmdseg[count - 1]), (int)sdslen(cmdstack));
            sdsfreesplitres(cmdseg, count);
        }
    }
    close_socket (ctx, client_socket);
    pthread_exit(NULL);
}


void close_socket (server_ctx *ctx, int client_socket)
{
    pthread_mutex_lock(&ctx->lock);
    close(client_socket);
    pthread_mutex_unlock(&ctx->lock);
    
}
