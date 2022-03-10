#ifndef SENDALL_H
#define SENDALL_H
#include <sys/types.h>
#include <sys/socket.h>
#include "../lib/sds/sds.h"
#include "server.h"
#include "log.h"
#include "reply.h"
#include "msg.h"

/* This sendall fucntion is cited from this link: https://beej.us/guide/bgnet/html/#sendall
 *
 * sendall - Send all msg to client
 *
 * s: client_socket
 *
 * buf: buffer message to be sent
 *
 * len: The length of the buffer message
 *
 * Return: -1 on failure, 0 on success
 *
 * The function is used make sure the buffer is fully sent
 */
int sendall(int s, char *buf, int *len);

/*
 * send_msg - A thread-safe wrapper around sendall, and replace calls
 * to sendall with calls to that function.
 *
 * client_socket: client_socket
 *
 * ctx: server_context, we use the socket_lock to protect send_all()
 *
 * msg: The buffer message to be sent
 *
 * Return: MSG_OK/MSG_ERROR
 *
 * The function is used to protect the sendall() function.
 */
int send_msg(int client_socket, server_ctx *ctx, sds msg);

/*
 * server_reply_nick - A thread-safe function to send NICK reply.
 *
 * ctx: server_context
 *
 * prefix: buffer message to be sent
 *
 * cmdtokens: tokenized command stacks
 *
 * argc: count of the argument numbers
 *
 * client_socket: client socket for the reply
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_nick(server_ctx *ctx,
                      sds prefix, sds *cmdtokens,
                      int argc, int client_socket);

/*
 * server_reply_quit_relay - A thread-safe function to relay QUIT reply.
 *
 * ctx: server_context
 *
 * prefix: buffer message to be sent
 *
 * cmdtokens: tokenized command stacks
 *
 * argc: count of the argument numbers
 *
 * client_socket: client socket for the reply
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_quit_relay(server_ctx *ctx,
                            sds prefix, sds *cmdtokens,
                            int argc, int client_socket);

/*
 * server_reply_quit - A thread-safe function to send QUIT reply.
 *
 * ctx: server_context
 *
 * cmdtokens: tokenized command stacks
 *
 * argc: count of the argument numbers
 *
 * client_hostname: client_hostname used to create the message
 *
 * client_socket: client socket for the reply
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_quit(server_ctx *ctx, sds *cmdtokens,
                      int argc, sds client_hostname,
                      int client_socket);

/*
 * server_reply_join - A thread-safe function to send JOIN reply.
 *
 * ctx: server_context
 *
 * prefix: buffer message to be sent
 *
 * cmd: reply_code
 *
 * nickname: nickname of the joined user
 *
 * channel_name: the name of the channel the user joined
 *
 * client_socket: client socket for the reply
 *
 * c: the channel the user joined
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_join(server_ctx *ctx,
                      sds prefix, char *cmd, sds nickname,
                      sds channel_name, int client_socket,
                      channel_t *c);

/*
 * server_reply_join_relay - A thread-safe function to relay JOIN reply.
 *
 * ctx: server_context
 *
 * join_prefix: buffer message to be sent
 *
 * cmdtokens: tokenized command stacks
 *
 * channel_name: the name of the channel the user joined
 *
 * client_socket: client socket for the reply
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_join_relay(server_ctx *ctx,
                            sds join_prefix, sds *cmdtokens,
                            sds channel_name, int client_socket);

/*
 * server_reply_privmsg - A thread-safe function to relay PRIVMSG reply.
 *
 * ctx: server_context
 *
 * prefix: buffer message to be sent
 *
 * cmdtokens: tokenized command stacks
 *
 * argc: count of the argument numbers
 *
 * client_socket: client socket for the reply
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_privmsg(server_ctx *ctx,
                         sds prefix, sds *cmdtokens, int argc,
                         int client_socket);

/*
 * server_reply_whois - A thread-safe function to relay WHOIS reply.
 *
 * ctx: server_context
 *
 * prefix: buffer message to be sent
 *
 * cmd: reply_code
 *
 * cmdtokens: tokenized command stacks
 *
 * conn: conn_info_t holding the servername and client_socket info
 *
 * starget: client searched in WHOIS command
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_whois(server_ctx *ctx,
                       sds prefix, sds cmd, sds *cmdtokens,
                       conn_info_t *conn, sds nickname,
                       client_t *starget);

/*
 * server_reply_ping - A thread-safe function to send PING reply.
 *
 * ctx: server_context
 *
 * prefix: buffer message to be sent
 *
 * server_hostname: the server_hostname to reply PING
 *
 * client_socket: client socket for the reply
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_ping(server_ctx *ctx, sds prefix,
                      sds server_hostname, int client_socket);

/*
 * server_reply_list - A thread-safe function to form LIST reply.
 *
 * ctx: server_context
 *
 * prefix: buffer message to be sent
 *
 * cmd: reply_code
 *
 * nickname: the nickname of the user to be listed
 *
 * channel_name: the channel_name
 *
 * num_client: the total number of users on that channel
 *
 * Return: sds string
 *
 */
sds server_reply_list(server_ctx *ctx,
                      sds prefix, char *cmd, sds nickname,
                      sds channel_name, sds num_client);

/*
 * server_reply_listend - A thread-safe function to send LIST end reply.
 *
 * ctx: server_context
 *
 * prefix: buffer message to be sent
 *
 * cmd: reply_code
 *
 * nickname: the nickname of the user to be listed
 *
 * client_socket: client socket for the reply
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_listend(server_ctx *ctx,
                         sds prefix, char *cmd, sds nickname,
                         int client_socket);

/*
 * server_reply_oper - A thread-safe function to send OPER reply.
 *
 * ctx: server_context
 *
 * prefix: buffer message to be sent
 *
 * cmd: reply_code
 *
 * cmdtokens: tokenized command stacks
 *
 * client_socket: client socket for the reply
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_oper(server_ctx *ctx,
                      sds prefix, char *cmd, sds *cmdtokens,
                      int client_socket);

/*
 * server_reply_part - A thread-safe function to send PART reply.
 *
 * ctx: server_context
 *
 * prefix: buffer message to be sent
 *
 * cmdtokens: tokenized command stacks
 *
 * channel_name: the channel_name
 *
 * argc: count of the argument numbers
 *
 * client_socket: client socket for the reply
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_part(server_ctx *ctx,
                      sds prefix, sds *cmdtokens, sds channel_name,
                      int argc, int client_socket);

/*
 * server_reply_mode - A thread-safe function to send MODE reply.
 *
 * ctx: server_context
 *
 * prefix: buffer message to be sent
 *
 * cmdtokens: tokenized command stacks
 *
 * client_socket: client socket for the reply
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_mode(server_ctx *ctx, sds prefix,
                      sds *cmdtokens, int client_socket);

/*
 * server_reply_welcome - A thread-safe function to send
 welcome reply for NICK and USER.
 *
 * ctx: server_context
 *
 * client: client information with NICK and USERNAME
 *
 * conn: connection information with serverhostname,
 * clienthostname and client_socket
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_welcome(server_ctx *ctx, client_t *client,
                         conn_info_t *conn);

/*
 * reply_welcome - A thread-safe function to send welcome
 * reply (not only for NICK and USER).
 *
 * ctx: server_context
 *
 * client: client information with NICK and USERNAME
 *
 * conn: connection information with serverhostname,
 * clienthostname and client_socket
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int reply_welcome(server_ctx *ctx, client_t *client, conn_info_t *conn);

/*
 * server_reply_lusers - A thread-safe function to send LUSERS reply.
 *
 * ctx: server_context
 *
 * client: client information with NICK and USERNAME
 *
 * conn: connection information with serverhostname,
 * clienthostname and client_socket
 *
 * Return: MSG_OK/MSG_ERROR
 *
 */
int server_reply_lusers(server_ctx *ctx, sds nick, conn_info_t *conn);

#endif
