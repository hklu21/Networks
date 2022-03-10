#include "send_msg.h"
#include "log.h"
#include "reply.h"
#include "msg.h"
#include "../lib/sds/sds.h"


int sendall(int s, char *buf, int *len)
{
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
    int total = 0;        // How many bytes we've sent
    int bytesleft = *len; // How many we have left to send
    int n;

    while (total < *len)
    {
        n = send(s, buf + total, bytesleft, 0);
        /* Check the return value of send(). */
        if (n == -1)
        {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total; // Return number actually sent here

    return n == -1 ? -1 : 0; // Return -1 on failure, 0 on success
}


int send_msg(int client_socket, server_ctx *ctx, sds msg)
{
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
    int r = MSG_OK;
    int len = sdslen(msg);

    pthread_mutex_lock(&ctx->socket_lock);
    if (sendall(client_socket, msg, &len) == -1)
    {
        chilog(ERROR, "We only sent %d bytes because of the error!\n", len);
        /* Check the return value of sendall(). */
        r = MSG_ERROR;
    }
    pthread_mutex_unlock(&ctx->socket_lock);

    return r;
}


int server_reply_nick(server_ctx *ctx, sds prefix, sds *cmdtokens,
                      int argc, int client_socket)
{
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

    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    chirc_message_construct(msg, prefix, cmdtokens[0]);

    sds param = sdsjoinsds(cmdtokens + 1, argc - 1, " ", 1);
    param = sdscat(param, "\r\n");
    chirc_message_add_parameter(msg, param, true);

    sds host_msg;
    chirc_message_to_string(msg, &host_msg);
    if (send_msg(client_socket, ctx, host_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(host_msg);
    sdsfree(param);

    chirc_message_destroy(msg);

    return MSG_OK;
}


int server_reply_quit_relay(server_ctx *ctx, sds prefix, sds *cmdtokens,
                            int argc, int client_socket)
{
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
    chirc_message_t *quit_msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    chirc_message_construct(quit_msg, prefix, cmdtokens[0]);

    sds param;
    if (argc > 1)
    {
        param = sdsjoinsds(cmdtokens + 1, argc - 1, " ", 1);
        if (param[0] == ':')
        {
            sdsrange(param, 1, sdslen(param));
        }
        param = sdscat(param, "\r\n");
    }
    else
    {
        param = sdscatprintf(sdsempty(), "Client Quit\r\n");
    }

    chirc_message_add_parameter(quit_msg, param, true);

    sds host_msg;
    chirc_message_to_string(quit_msg, &host_msg);
    if (send_msg(client_socket, ctx, host_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(host_msg);
    sdsfree(param);
    chirc_message_destroy(quit_msg);

    return MSG_OK;
}


int server_reply_quit(server_ctx *ctx, sds *cmdtokens,
                      int argc, sds client_hostname, int client_socket)
{
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

    sds msg = sdsempty();

    if (argc - 1 == 0)
    {
        msg = "Client Quit";
    }
    else
    {
        msg = sdsjoinsds(cmdtokens + 1, argc - 1, " ", 1);
    }

    if (msg[0] == ':')
    {
        sdsrange(msg, 1, sdslen(msg));
    }
    sds reply_msg = sdscatprintf(sdsempty(),
                                 "ERROR :Closing Link: %s (%s)\r\n",
                                 client_hostname,
                                 msg);

    if (send_msg(client_socket, ctx, reply_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(reply_msg);

    return MSG_OK;
}


int server_reply_join(server_ctx *ctx,
                      sds prefix, char *cmd, sds nickname, sds channel_name,
                      int client_socket, channel_t *c)
{

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

    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    chirc_message_construct(msg, prefix, cmd);

    chirc_message_add_parameter(msg, nickname, false);

    sds param = sdsempty();

    if (!strncmp(cmd, RPL_NAMREPLY, MAX_STR_LEN))
    {
        /* RPL_NAMREPLY */
        chirc_message_add_parameter(msg, "=", false);
        sdscatsds(param, channel_name);
        chirc_message_add_parameter(msg, param, false);

        /* Add name list as param */
        param = sdscpy(param, "@");

        pthread_mutex_lock(&ctx->channels_lock);
        for (channel_client *cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
        {
            param = sdscatsds(param, cc->nick);
            if (cc->hh.next)
            {
                param = sdscat(param, " ");
            }
        }
        pthread_mutex_unlock(&ctx->channels_lock);

        param = sdscat(param, "\r\n");
    }
    else if (!strncmp(cmd, RPL_ENDOFNAMES, MAX_STR_LEN))
    {
        /* RPL_ENDOFNAMES */
        sdscatsds(param, channel_name);
        chirc_message_add_parameter(msg, param, false);
        param = sdscpy(param, "End of NAMES list\r\n");
    }

    chirc_message_add_parameter(msg, param, true);

    sds host_msg;
    chirc_message_to_string(msg, &host_msg);
    if (send_msg(client_socket, ctx, host_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(param);
    sdsfree(host_msg);
    chirc_message_destroy(msg);
    return MSG_OK;
}


int server_reply_join_relay(server_ctx *ctx,
                            sds join_prefix, sds *cmdtokens, sds channel_name,
                            int client_socket)
{
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

    chirc_message_t *join_msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    chirc_message_construct(join_msg, join_prefix, cmdtokens[0]);

    sds join_param = sdsnew(channel_name);
    join_param = sdscat(join_param, "\r\n");
    chirc_message_add_parameter(join_msg, join_param, false);

    sds host_join_msg;
    chirc_message_to_string(join_msg, &host_join_msg);
    if (send_msg(client_socket, ctx, host_join_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(host_join_msg);
    sdsfree(join_param);
    chirc_message_destroy(join_msg);

    return MSG_OK;
}


int server_reply_privmsg(server_ctx *ctx,
                         sds prefix, sds *cmdtokens, int argc, int client_socket)
{
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
    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    sds cmd = cmdtokens[0];
    if (!strncmp(cmdtokens[0], "NOTICE", MAX_STR_LEN) &&
        cmdtokens[1][0] == '#')
    {
        cmd = "PRIVMSG";
    }
    chirc_message_construct(msg, prefix, cmd);

    chirc_message_add_parameter(msg, cmdtokens[1], false);
    sds param = sdsjoinsds(cmdtokens + 2, argc - 2, " ", 1);
    if (param[0] == ':')
    {
        sdsrange(param, 1, sdslen(param));
    }
    param = sdscat(param, "\r\n");

    chirc_message_add_parameter(msg, param, true);

    sds host_msg;
    chirc_message_to_string(msg, &host_msg);
    if (send_msg(client_socket, ctx, host_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(host_msg);
    sdsfree(param);
    chirc_message_destroy(msg);

    return MSG_OK;
}


int server_reply_ping(server_ctx *ctx, sds prefix, sds server_hostname, int client_socket)
{
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

    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    chirc_message_construct(msg, prefix, "PONG");

    sds host = sdscatsds(server_hostname, sdsnew("\r\n"));
    chirc_message_add_parameter(msg, host, true);

    sds host_msg;
    chirc_message_to_string(msg, &host_msg);
    if (send_msg(client_socket, ctx, host_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(host_msg);
    sdsfree(host);
    chirc_message_destroy(msg);

    return MSG_OK;
}


int server_reply_whois(server_ctx *ctx,
                       sds prefix, sds cmd, sds *cmdtokens, conn_info_t *conn,
                       sds nickname, client_t *starget)
{
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
    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    chirc_message_construct(msg, prefix, cmd);

    chirc_message_add_parameter(msg, nickname, false);

    chirc_message_add_parameter(msg, cmdtokens[1], false);

    if (!strncmp(cmd, RPL_WHOISUSER, MAX_STR_LEN))
    {
        chirc_message_add_parameter(msg, starget->info.username, false);
        chirc_message_add_parameter(msg, conn->client_hostname, false);
        chirc_message_add_parameter(msg, "*", false);

        sds param = sdsnew(starget->info.realname);
        param = sdscat(param, "\r\n");
        chirc_message_add_parameter(msg, param, true);

        sdsfree(param);
    }
    else if (!strncmp(cmd, RPL_WHOISSERVER, MAX_STR_LEN))
    {
        chirc_message_add_parameter(msg, conn->server_hostname, false);

        chirc_message_add_parameter(msg, "*\r\n", true);
    }
    else if (!strncmp(cmd, RPL_ENDOFWHOIS, MAX_STR_LEN))
    {
        chirc_message_add_parameter(msg, "End of WHOIS list\r\n", true);
    }

    sds host_msg_user;

    chirc_message_to_string(msg, &host_msg_user);
    if (send_msg(conn->client_socket, ctx, host_msg_user) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    chirc_message_destroy(msg);
    sdsfree(host_msg_user);
    return MSG_OK;
}


sds server_reply_list(server_ctx *ctx,
                      sds prefix, char *cmd, sds nickname, sds channel_name,
                      sds num_client)
{
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
    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    chirc_message_construct(msg, prefix, cmd);

    chirc_message_add_parameter(msg, nickname, false);
    chirc_message_add_parameter(msg, channel_name, false);
    chirc_message_add_parameter(msg, num_client, false);
    chirc_message_add_parameter(msg, "\r\n", true);

    sds host_msg;
    chirc_message_to_string(msg, &host_msg);

    chirc_message_destroy(msg);

    return host_msg;
}


int server_reply_listend(server_ctx *ctx,
                         sds prefix, char *cmd, sds nickname, int client_socket)
{
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
    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    chirc_message_construct(msg, prefix, cmd);

    chirc_message_add_parameter(msg, nickname, false);

    chirc_message_add_parameter(msg, "End of LIST\r\n", true);

    sds host_msg;
    chirc_message_to_string(msg, &host_msg);
    // chilog(TRACE, "%s\n", host_msg);

    if (send_msg(client_socket, ctx, host_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(host_msg);
    chirc_message_destroy(msg);

    return MSG_OK;
}


int server_reply_oper(server_ctx *ctx,
                      sds prefix, char *cmd, sds *cmdtokens, int client_socket)
{
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

    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    chirc_message_construct(msg, prefix, cmd);

    chirc_message_add_parameter(msg, cmdtokens[1], false);

    chirc_message_add_parameter(msg, "You are now an IRC operator\r\n", true);
    sds host_msg;

    chirc_message_to_string(msg, &host_msg);

    if (send_msg(client_socket, ctx, host_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(host_msg);
    chirc_message_destroy(msg);

    return MSG_OK;
}


int server_reply_part(server_ctx *ctx,
                      sds prefix, sds *cmdtokens, sds channel_name, int argc,
                      int client_socket)
{
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

    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    chirc_message_construct(msg, prefix, cmdtokens[0]);

    if (argc > 2) /* If has part msg */
    {
        chirc_message_add_parameter(msg, channel_name, false);

        sds part_msg = sdsempty();
        part_msg = sdsjoinsds(cmdtokens + 2, argc - 2, " ", 1);
        if (part_msg[0] == ':')
            sdsrange(part_msg, 1, sdslen(part_msg));
        part_msg = sdscat(part_msg, "\r\n");
        chirc_message_add_parameter(msg, part_msg, true);

        sdsfree(part_msg);
    }
    else
    {
        sds param = sdscatprintf(sdsempty(), "%s\r\n", channel_name);
        chirc_message_add_parameter(msg, param, false);

        sdsfree(param);
    }

    sds host_msg;
    chirc_message_to_string(msg, &host_msg);
    if (send_msg(client_socket, ctx, host_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(host_msg);
    chirc_message_destroy(msg);

    return MSG_OK;
}


int server_reply_mode(server_ctx *ctx, sds prefix, sds *cmdtokens,
                      int client_socket)
{
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
    sds host_msg = sdscatprintf(sdsempty(),
                                "%s %s %s %s %s\r\n",
                                prefix,
                                cmdtokens[0],
                                cmdtokens[1],
                                cmdtokens[2],
                                cmdtokens[3]);

    if (send_msg(client_socket, ctx, host_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(host_msg);

    return MSG_OK;
}


int server_reply_welcome(server_ctx *ctx, client_t *client, conn_info_t *conn)
{
    /*
     * server_reply_welcome - A thread-safe function to send welcome reply for NICK and USER.
     *
     * ctx: server_context
     *
     * client: client information with NICK and USERNAME
     *
     * conn: connection information with serverhostname, clienthostname and client_socket
     *
     * Return: MSG_OK/MSG_ERROR
     *
     */
    sds msg = sdscatprintf(sdsempty(),
                           ":%s %s %s :Welcome to the Internet Relay Network %s!%s@%s\r\n",
                           conn->server_hostname,
                           RPL_WELCOME,
                           client->info.nick,
                           client->info.nick,
                           client->info.username,
                           conn->client_hostname);

    if (send_msg(conn->client_socket, ctx, msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(msg);
    return MSG_OK;
}


int reply_welcome(server_ctx *ctx, client_t *client, conn_info_t *conn)
{
    /*
     * reply_welcome - A thread-safe function to send welcome reply (not only for NICK and USER).
     *
     * ctx: server_context
     *
     * client: client information with NICK and USERNAME
     *
     * conn: connection information with serverhostname, clienthostname and client_socket
     *
     * Return: MSG_OK/MSG_ERROR
     *
     */
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;

    /* RPL_YOURHOST */
    sds host_msg = sdscatprintf(sdsempty(),
                                ":%s %s %s :Your host is %s, running version %s\r\n",
                                server_hostname, RPL_YOURHOST, client->info.nick, server_hostname, VERSION);
    if (send_msg(client_socket, ctx, host_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    /* RPL_CREATED */
    sds create_msg = sdscatprintf(sdsempty(),
                                  ":%s %s %s :This server was created %d-%02d-%02d %02d:%02d:%02d\r\n",
                                  server_hostname,
                                  RPL_CREATED,
                                  client->info.nick,
                                  tm.tm_year + 1900,
                                  tm.tm_mon + 1,
                                  tm.tm_mday,
                                  tm.tm_hour,
                                  tm.tm_min,
                                  tm.tm_sec);
    if (send_msg(client_socket, ctx, create_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    /*RPL_MYINFO */
    sds info_msg = sdscatprintf(sdsempty(), ":%s %s %s %s %s %s %s\r\n",
                                server_hostname, RPL_MYINFO, client->info.nick, server_hostname,
                                VERSION, "ao", "mtov");
    if (send_msg(client_socket, ctx, info_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(host_msg);
    sdsfree(create_msg);
    sdsfree(info_msg);

    return MSG_OK;
}


int server_reply_lusers(server_ctx *ctx, sds nick, conn_info_t *conn)
{
    /*
     * server_reply_lusers - A thread-safe function to send LUSERS reply.
     *
     * ctx: server_context
     *
     * client: client information with NICK and USERNAME
     *
     * conn: connection information with serverhostname, clienthostname and client_socket
     *
     * Return: MSG_OK/MSG_ERROR
     *
     */
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;

    /* Count number of users */
    pthread_mutex_lock(&ctx->clients_lock);
    int num_connections = HASH_COUNT(ctx->client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);

    /* Count num_connected_users, num_of_total_connections */
    pthread_mutex_lock(&ctx->lock);
    int num_of_users = ctx->num_connected_users;
    int num_of_total_connections = ctx->total_connections;
    pthread_mutex_unlock(&ctx->lock);

    int num_of_unknown_connections = num_of_total_connections - num_connections;

    /* RPL_LUSERCLIENT */
    sds serclient_msg = sdscatprintf(sdsempty(),
                                     ":%s %s %s :There are %d users and 0 services on 1 servers\r\n",
                                     server_hostname,
                                     RPL_LUSERCLIENT,
                                     nick,
                                     num_of_users);

    if (send_msg(client_socket, ctx, serclient_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    /* RPL_LUSEROP */
    irc_oper_t **irc_operators_hashtable = &ctx->irc_operators_hashtable;
    /* Count num_of_irc_operator */
    pthread_mutex_lock(&ctx->operators_lock);
    int num_of_irc_operator = HASH_COUNT(ctx->irc_operators_hashtable);
    pthread_mutex_unlock(&ctx->operators_lock);

    sds serop_msg = sdscatprintf(sdsempty(),
                                 ":%s %s %s %d :operator(s) online\r\n",
                                 server_hostname,
                                 RPL_LUSEROP,
                                 nick,
                                 num_of_irc_operator);
    if (send_msg(client_socket, ctx, serop_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    /* RPL_LUSERUNKNOWN */
    sds serunknown_msg = sdscatprintf(sdsempty(),
                                      ":%s %s %s %d :unknown connection(s)\r\n",
                                      server_hostname,
                                      RPL_LUSERUNKNOWN,
                                      nick,
                                      num_of_unknown_connections);
    if (send_msg(client_socket, ctx, serunknown_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    /* RPL_LUSERCHANNELS */
    channel_t **channel_hashtable = &ctx->channels_hashtable;

    /* Count num_of_channels */
    pthread_mutex_lock(&ctx->channels_lock);
    int num_of_channels = HASH_COUNT(ctx->channels_hashtable);
    pthread_mutex_unlock(&ctx->channels_lock);

    sds serchannel_msg = sdscatprintf(sdsempty(), ":%s %s %s %d :channels formed\r\n",
                                      server_hostname,
                                      RPL_LUSERCHANNELS,
                                      nick,
                                      num_of_channels);
    if (send_msg(client_socket, ctx, serchannel_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    /* RPL_LUSERME */
    sds serme_msg = sdscatprintf(sdsempty(),
                                 ":%s %s %s :I have %d clients and 1 servers\r\n",
                                 server_hostname,
                                 RPL_LUSERME,
                                 nick,
                                 num_connections);
    if (send_msg(client_socket, ctx, serme_msg) == MSG_ERROR)
    {
        return MSG_ERROR;
    }

    sdsfree(serclient_msg);
    sdsfree(serop_msg);
    sdsfree(serunknown_msg);
    sdsfree(serchannel_msg);
    sdsfree(serme_msg);

    return MSG_OK;
}