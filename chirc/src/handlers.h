#ifndef HANDLERS_H
#define HANDLERS_H
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
#include "reply.h"
#include "server.h"
#include "client.h"
#include "channels.h"

#define VERSION "99"
#define CLIENTHOST "foo.example.com"
#define SEVERHOST "bar.example.com"

int handle_request(server_ctx *ctx,int client_socket, sds *cmdtokens, int argc, char *server_hostname);

int handle_NICK(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_USER(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_QUIT(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_JOIN(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_PRIVMSG(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_NOTICE(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_PING(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_PONG(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_LUSERS(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_WHOIS(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_LIST(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_PART(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_MODE(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);
int handle_OPER(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname);

void reply_registration(server_ctx *ctx, sds *cmd, int argc, int client_socket, client_t ** client_hashtable, char *server_hostname);

typedef int (*handler_function)(server_ctx *ctx,  int client_socket, sds *token, int argc, char *server_hostname);

struct handler_entry
{
    char *name;
    handler_function func;
};



#define NICK_PARAMETER_NUM 1
#define USER_PARAMETER_NUM 4 
#define JOIN_PARAMETER_NUM 1
#define PRIVMSG_PARAMETER_NUM 2
#define NOTICE_PARAMETER_NUM 2
#define WHOIS_PARAMETER_NUM 1
#define PART_PARAMETER_NUM 1
#define OPER_PARAMETER_NUM 2


#endif