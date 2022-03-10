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

/*
 * handle_request -  handler the request of different commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 */
int handle_request(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_NICK -  handler the NICK commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_NICK(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_USER -  handler the USER commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_USER(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_QUIT -  handler the QUIT commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_QUIT(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_JOIN -  handler the JOIN commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_JOIN(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_PRIVMSG -  handler the PRIVMSG commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_PRIVMSG(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_NOTICE -  handler the NOTICE commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_NOTICE(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_PING -  handler the PING commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_PING(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_PONG -  handler the PONG commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_PONG(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_LUSERS -  handler the LUSERS commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_LUSERS(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_WHOIS -  handler the WHOIS commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_WHOIS(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_LIST -  handler the LIST commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_LIST(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_PART -  handler the PART commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_PART(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_MODE -  handler the MODE commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_MODE(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

/*
 * handle_OPER -  handler the OPER commands
 *
 * ctx: The server context
 *
 * cmdtokens: command stacks created from recved messages
 *
 * argc: the count of arguments from the command stacks
 *
 * conn: the conn_info_t object
 *
 * Return: CHIRC_OK/CHIRC_ERROR
 *
 */
int handle_OPER(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

typedef int (*handler_function)(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn);

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
#define MODE_PARAMETER_NUM 3

#endif