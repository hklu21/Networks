#include "handlers.h"
#include "msg.h"
#include "log.h"
#include "reply.h"
#include "../lib/uthash.h"
#include "../lib/sds/sds.h"
#include "channels.h"
#include "server_cmd.h"
#include "send_msg.h"

/* Dispatch table */
struct handler_entry handlers[] = {
    {"NICK", handle_NICK},
    {"USER", handle_USER},
    {"QUIT", handle_QUIT},
    {"JOIN", handle_JOIN},
    {"PRIVMSG", handle_PRIVMSG},
    {"NOTICE", handle_NOTICE},
    {"PING", handle_PING},
    {"PONG", handle_PONG},
    {"LUSERS", handle_LUSERS},
    {"WHOIS", handle_WHOIS},
    {"LIST", handle_LIST},
    {"MODE", handle_MODE},
    {"OPER", handle_OPER},
    {"PART", handle_PART},
};


int handle_request(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int j, rc = 0;
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;
    int num_handlers = sizeof(handlers) / sizeof(struct handler_entry);

    for (j = 0; j < num_handlers; j++)
    {
        if (!strncmp(handlers[j].name, cmdtokens[0], MAX_STR_LEN))
        {
            rc = handlers[j].func(ctx, cmdtokens, argc, conn);
            break;
        }
    }

    /* Thread-safe call with a lock wrapped around the find_USER function */
    client_t *s = server_find_USER(ctx, client_socket);

    if (s == NULL)
    {
        /* Registration Failed */
        return CHIRC_ERROR;
    }

    if (rc == CHIRC_ERROR)
    {
        /* Registration Failed */
        return CHIRC_ERROR;
    }

    if (s->info.state == REGISTERED &&
        strncmp(cmdtokens[0], "PONG", MAX_STR_LEN) &&
        strncmp(cmdtokens[0], "PRIVMSG", MAX_STR_LEN) &&
        strncmp(cmdtokens[0], "PART", MAX_STR_LEN) &&
        strncmp(cmdtokens[0], "NOTICE", MAX_STR_LEN) &&
        strncmp(cmdtokens[0], "WHOIS", MAX_STR_LEN) &&
        strncmp(cmdtokens[0], "JOIN", MAX_STR_LEN) &&
        strncmp(cmdtokens[0], "OPER", MAX_STR_LEN) &&
        strncmp(cmdtokens[0], "MODE", MAX_STR_LEN))
    {
        if (j == num_handlers) // Unknown command
        {
            reply_error(cmdtokens, ERR_UNKNOWNCOMMAND, conn, ctx);

            return CHIRC_ERROR;
        }
        if (!strncmp(cmdtokens[0], "USER", MAX_STR_LEN) ||
            !strncmp(cmdtokens[0], "NICK", MAX_STR_LEN))
        {
            if (server_reply_welcome(ctx, s, conn) == MSG_ERROR)
            {
                return CHIRC_ERROR;
            }
        }

        if (reply_welcome(ctx, s, conn) == MSG_ERROR)
        {
            return CHIRC_ERROR;
        }

        if (handle_LUSERS(ctx, cmdtokens, argc, conn) == MSG_ERROR)
        {
            return CHIRC_ERROR;
        }

        if (reply_error(cmdtokens, ERR_NOMOTD, conn, ctx) == MSG_ERROR)
        {
            return CHIRC_ERROR;
        }
    }

    return CHIRC_OK;
}


int handle_NICK(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;

    /* Implement the ERR_NONICKNAMEGIVEN */
    if (argc - 1 < NICK_PARAMETER_NUM)
    {
        reply_error(cmdtokens, ERR_NONICKNAMEGIVEN, conn, ctx);

        return CHIRC_ERROR;
    }

    /* Thread-safe call to server nickname from nick_hashtable */
    nick_t *find = server_find_NICK(ctx, cmdtokens[1]);

    if (find != NULL)
    {
        /* ERR_NICKNAMEINUSE */
        reply_error(cmdtokens, ERR_NICKNAMEINUSE, conn, ctx);

        return CHIRC_ERROR;
    }


    /* Tread-safe function to find_USER */
    client_t *s = server_find_USER(ctx, client_socket);

    /* We added new user in client hashtable whenever we have a new nickname. */
    if (s == NULL)
    {
        /* First time user */
        s = malloc(sizeof(client_t));
        s->info.nick = sdsempty();
        s->info.realname = sdsempty();
        s->info.username = sdsempty();
        s->info.state = NOT_REGISTERED;
        s->socket = client_socket;
    }

    if (argc - 1 < NICK_PARAMETER_NUM)
    {
        /* ERR_NONICKNAMEGIVEN */
        reply_error(cmdtokens, ERR_NONICKNAMEGIVEN, conn, ctx);

        return CHIRC_ERROR;
    }

    if (s->info.state == NICK_MISSING)
    {
        s->info.state = REGISTERED;
        s->info.nick = sdscpy(s->info.nick, cmdtokens[1]);

        /* Tread-safe function to add connected user number */
        add_connected_user_number(ctx);

        /* Thread-safe call to add NICK to nick_hashtable */
        server_add_NICK(ctx, s->socket, s->info.nick);

        /* reply_registration(ctx, cmdtokens, argc, client_socket, client_hashtable, server_hostname); */
        return REGISTERED;
    }
    else if (s->info.state == REGISTERED)
    {
        /* Update NICK */
        /* Reply to self */
        sds prefix = sdscatprintf(sdsempty(), ":%s!%s@%s",
                                  s->info.nick,
                                  s->info.username,
                                  client_hostname);

        if (server_reply_nick(ctx, prefix, cmdtokens, argc, client_socket) == MSG_ERROR)
        {
            return CHIRC_ERROR;
        }

        /* Reply nick update to channels */
        channel_t *c;

        pthread_mutex_lock(&ctx->channels_lock);
        for (c = ctx->channels_hashtable; c != NULL; c = c->hh.next)
        {
            channel_client *cc = NULL;
            cc = find_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
            if (cc == NULL) // Not in the channel
                continue;
            for (cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
            {
                /* Reply to the clients in the channel */

                /* Do not send msg to self more than once*/
                if (!strncmp(cc->nick, s->info.nick, MAX_STR_LEN))
                {
                    continue;
                }
                /* Tread-safe call to find_NICK */
                nick_t *msgtarget = server_find_NICK(ctx, cc->nick);

                if (server_reply_nick(ctx, prefix, cmdtokens, argc, msgtarget->client_socket) == MSG_ERROR)
                {
                    return CHIRC_ERROR;
                }
            }
        }
        pthread_mutex_unlock(&ctx->channels_lock);

        sdsfree(prefix);

        s->info.nick = sdscpy(s->info.nick, cmdtokens[1]);
        /* Update nick hashtable */
        /* Tread-safe call to remove_NICK */
        server_remove_NICK(ctx, s->info.nick);
        /* Tread-safe call to add_NICK */
        server_add_NICK(ctx, client_socket, s->info.nick);

        return REGISTERED;
    }
    else if (s->info.state == NOT_REGISTERED)
    {
        s->info.state = USER_MISSING;
        s->info.nick = sdscpy(s->info.nick, cmdtokens[1]);
        /* Tread-safe call to add_USER */
        server_add_USER(ctx, s, client_socket);

        return NOT_REGISTERED;
    }

    s->info.nick = sdscpy(s->info.nick, cmdtokens[1]);

    return NOT_REGISTERED;
}


int handle_USER(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;

    /*Implement the ERR_NEEDMOREPARAMS */
    if (argc - 1 < USER_PARAMETER_NUM)
    {
        reply_error(cmdtokens, ERR_NEEDMOREPARAMS, conn, ctx);

        return CHIRC_ERROR;
    }
    /* Thread-safe call to find_USER */
    client_t *s = server_find_USER(ctx, client_socket);

    if (s != NULL && s->info.state == REGISTERED)
    {
        /* USER is already registered, ERR_ALREADYREGISTRED */
        return REGISTERED;
    }

    if (s == NULL)
    {
        s = malloc(sizeof(client_t));
        /* USER_NOT_FOUND, create new user. */
        s->info.username = sdsempty();
        s->info.realname = sdsempty();
        s->info.nick = sdsempty();
        s->socket = client_socket;
        s->info.state = NOT_REGISTERED;
    }

    s->info.username = sdscpy(s->info.username, cmdtokens[1]);

    sds realname = sdsjoinsds(cmdtokens + 4, argc - 4, " ", 1);

    if (realname[0] == ':')
    {
        sdsrange(realname, 1, sdslen(realname));
    }

    s->info.realname = sdscpy(s->info.realname, realname);

    if ((int)sdslen(s->info.username) == 0)
    {
        /* Not handled USER message sussessfully */
        return CHIRC_ERROR;
    }

    if (s->info.state == USER_MISSING)
    {
        s->info.state = REGISTERED;
        /* Thread-safe call to add_NICK */
        server_add_NICK(ctx, client_socket, s->info.nick);
        // reply_registration(ctx, cmdtokens, argc, client_socket, client_hashtable, server_hostname);
        
        /* Tread-safe function to add connected user number */
        add_connected_user_number(ctx);
        
        return REGISTERED;
    }
    else if (s->info.state == NOT_REGISTERED)
    {
        s->info.state = NICK_MISSING;
        /* Thread-safe call to add_USER */
        server_add_USER(ctx, s, client_socket);

        return NICK_MISSING;
    }

    return NICK_MISSING;
}


int handle_QUIT(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;

    dec_total_connected_number(ctx);

    /* Thread-safe call to find_USER */
    client_t *s = server_find_USER(ctx, client_socket);

    if (s == NULL) // Not registered
    {
        /* ERR_NOTREGISTERED */
        reply_error(cmdtokens, ERR_NOTREGISTERED, conn, ctx);

        return CHIRC_ERROR;
    }
    if (s->info.state != REGISTERED) // Not registered
    {
        /* ERR_NOTREGISTERED */
        reply_error(cmdtokens, ERR_NOTREGISTERED, conn, ctx);

        return CHIRC_ERROR;
    }

    dec_connected_user_number(ctx);

    if (server_reply_quit(ctx, cmdtokens, argc, client_hostname, client_socket) == MSG_ERROR)
    {
        return CHIRC_ERROR;
    }

    // Reply nick update to channels
    sds prefix = sdscatprintf(sdsempty(), ":%s!%s@%s",
                              s->info.nick,
                              s->info.username,
                              client_hostname);

    pthread_mutex_lock(&ctx->channels_lock);
    for (channel_t *c = ctx->channels_hashtable; c != NULL; c = c->hh.next)
    {
        channel_client *cc = NULL;
        cc = find_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
        if (cc == NULL) // Not in the channel
            continue;
        for (cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
        {
            /* Reply to the clients in the channel */
            /* Do not send msg to self more than once*/
            if (!strncmp(cc->nick, s->info.nick, MAX_STR_LEN))
            {
                continue;
            }
            /* Thread-safe call to find_NICK */
            nick_t *msgtarget = server_find_NICK(ctx, cc->nick);
            if (server_reply_quit_relay(ctx, prefix, cmdtokens, argc, msgtarget->client_socket) == MSG_ERROR)
            {
                return CHIRC_ERROR;
            }
        }
        remove_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
        if (HASH_COUNT(c->channel_clients) <= 0)
        {
            remove_CHANNEL(c->channel_name, &ctx->channels_hashtable);
        }
    }
    pthread_mutex_unlock(&ctx->channels_lock);

    sdsfree(prefix);
    close_socket(ctx, client_socket);
    pthread_exit(NULL);
    return CHIRC_OK;
}


int handle_JOIN(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;
    sds channel_name = cmdtokens[1];

    /* Thread-safe call to find_USER */
    client_t *s = server_find_USER(ctx, client_socket);
    if (s == NULL) // Not registered
    {
        /* ERR_NOTREGISTERED */
        reply_error(cmdtokens, ERR_NOTREGISTERED, conn, ctx);

        return CHIRC_ERROR;
    }
    if (s->info.state != REGISTERED) // Not registered
    {
        /* ERR_NOTREGISTERED */
        reply_error(cmdtokens, ERR_NOTREGISTERED, conn, ctx);

        return CHIRC_ERROR;
    }
    /* ERR_NONICKNAMEGIVEN */
    if (argc - 1 < JOIN_PARAMETER_NUM)
    {
        reply_error(cmdtokens, ERR_NEEDMOREPARAMS, conn, ctx);

        return CHIRC_ERROR;
    }

    /* Thread-safe call to find_CHANNEL */
    channel_t *c = server_find_CHANNEL(ctx, channel_name);
    bool flag = 1; // If flag == 1, channel exists, else channel is newly created.
    if (c == NULL)
    {
        /* Channel not exist */
        /* Thread-safe call to add_CHANNEL */
        c = server_add_CHANNEL(ctx, channel_name);
        flag = 0;
    }

    /* Thread-safe call to find_CHANNEL_CLIENT */
    channel_client *cc = server_find_CHANNEL_CLIENT(ctx, c, s->info.nick);
    if (cc != NULL)
    {
        /* Client already in the channel */
        return CHIRC_ERROR;
    }

    // /* Thread-safe call to add client to channel */
    cc = server_add_CHANNEL_CLIENT(ctx, s->info.nick, channel_name, flag);

    /* Send JOIN msg to each client in the channel */
    sds join_prefix = sdscatprintf(sdsempty(), ":%s!%s@%s",
                                   s->info.nick,
                                   s->info.username,
                                   client_hostname);

    pthread_mutex_lock(&ctx->channels_lock);
    for (cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
    {
        /* Send JOIN msg to each client in the channel */
        /*Thread-safe call to find_NICK*/
        nick_t *msgtarget = server_find_NICK(ctx, cc->nick);

        if (server_reply_join_relay(ctx, join_prefix, cmdtokens, channel_name,
                                    msgtarget->client_socket) == MSG_ERROR)
        {
            return CHIRC_ERROR;
        }
    }
    pthread_mutex_unlock(&ctx->channels_lock);

    sdsfree(join_prefix);

    /* RPL_NAMREPLY */
    char *prefix = sdscatsds(sdsnew(":"), server_hostname);
    if (server_reply_join(ctx, prefix, RPL_NAMREPLY, s->info.nick,
                          channel_name, client_socket, c) == MSG_ERROR)
    {
        return CHIRC_ERROR;
    }

    /* RPL_ENDOFNAMES */
    if (server_reply_join(ctx, prefix, RPL_ENDOFNAMES, s->info.nick,
                          channel_name, client_socket, c) == MSG_ERROR)
    {
        return CHIRC_ERROR;
    }

    sdsfree(prefix);

    return CHIRC_OK;
}


int handle_PRIVMSG(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;

    /*Thread-safe call to find_USER*/
    client_t *s = server_find_USER(ctx, client_socket);

    if (s == NULL) // not registered
    {
        /* ERR_NOTREGISTERED */
        reply_error(cmdtokens, ERR_NOTREGISTERED, conn, ctx);

        return CHIRC_ERROR;
    }
    if (s->info.state != REGISTERED) // not registered
    {
        /* ERR_NOTREGISTERED */
        reply_error(cmdtokens, ERR_NOTREGISTERED, conn, ctx);

        return CHIRC_ERROR;
    }

    if (argc - 1 < PRIVMSG_PARAMETER_NUM)
    {
        if (argc == 1)
        {
            /* ERR_NORECIPIENT */
            reply_error(cmdtokens, ERR_NORECIPIENT, conn, ctx);

            return CHIRC_ERROR;
        }
        else if (argc == 2)
        {
            reply_error(cmdtokens, ERR_NOTEXTTOSEND, conn, ctx);

            return CHIRC_ERROR;
        }
    }

    /* PRIVMSG to channels */
    if (cmdtokens[1][0] == '#')
    {
        /* Thread-safe call to find_CHANNEL */
        channel_t *c = server_find_CHANNEL(ctx, cmdtokens[1]);
        if (c == NULL) // Channel not exist
        {
            reply_error(cmdtokens, ERR_NOSUCHNICK, conn, ctx);

            return CHIRC_ERROR;
        }
        /* Thread-safe call to find_CHANNEL_CLIENT */
        channel_client *cc = server_find_CHANNEL_CLIENT(ctx, c, s->info.nick);

        if (cc == NULL) // Client not in the channel
        {
            reply_error(cmdtokens, ERR_CANNOTSENDTOCHAN, conn, ctx);
            return CHIRC_ERROR;
        }

        sds prefix = sdscatprintf(sdsempty(), ":%s!%s@%s",
                                  s->info.nick,
                                  s->info.username,
                                  client_hostname);

        pthread_mutex_lock(&ctx->channels_lock);
        /* Send msg to each client in the channel */
        for (cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
        {
            /* Do not send msg to self */
            if (!strncmp(cc->nick, s->info.nick, MAX_STR_LEN))
            {
                continue;
            }
            /* Thread-safe call to find_NICK */
            nick_t *msgtarget = server_find_NICK(ctx, cc->nick);

            if (server_reply_privmsg(ctx, prefix, cmdtokens, argc, msgtarget->client_socket) == MSG_ERROR)
            {
                return CHIRC_ERROR;
            }
        }
        pthread_mutex_unlock(&ctx->channels_lock);

        sdsfree(prefix);

        return CHIRC_OK;
    }
    /* Thread-safe call to find_NICK */
    nick_t *msgtarget = server_find_NICK(ctx, cmdtokens[1]);

    if (msgtarget == NULL) // Nickname not exist
    {
        reply_error(cmdtokens, ERR_NOSUCHNICK, conn, ctx);

        return CHIRC_ERROR;
    }

    sds msg_prefix = sdscatprintf(sdsempty(), ":%s!%s@%s",
                                  s->info.nick,
                                  s->info.username,
                                  client_hostname); // reply msg prefix

    if (server_reply_privmsg(ctx, msg_prefix, cmdtokens, argc, msgtarget->client_socket) == MSG_ERROR)
    {
        return CHIRC_ERROR;
    }

    return CHIRC_OK;
}


int handle_NOTICE(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;

    /* Thread-safe call to find_USER */
    client_t *s = server_find_USER(ctx, client_socket);

    if (s == NULL) // Not registered
    {
        /* ERR_NOTREGISTERED */
        return CHIRC_ERROR;
    }

    if (s->info.state != REGISTERED) // Not registered
    {
        return CHIRC_ERROR;
    }

    if (argc - 1 < PRIVMSG_PARAMETER_NUM)
    {
        return CHIRC_ERROR;
    }

    /* NOTICE to channels */
    if (cmdtokens[1][0] == '#')
    {
        /* Thread-safe call to find_CHANNEL */
        channel_t *c = server_find_CHANNEL(ctx, cmdtokens[1]);
        if (c == NULL) // Channel not exist
        {
            /* ERR_NOSUCHCHANNEL */
            return CHIRC_ERROR;
        }
        /* Thread-safe call to find_CHANNEL_CLIENT */
        channel_client *cc = server_find_CHANNEL_CLIENT(ctx, c, s->info.nick);

        if (cc == NULL) // Client not in the channel
        {
            /* ERR_CANNOTSENDTOCHAN */
            return CHIRC_ERROR;
        }

        /* Send msg to each client in the channel */
        pthread_mutex_lock(&ctx->channels_lock);
        for (cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
        {

            /* Do not send msg to self */
            if (!strncmp(cc->nick, s->info.nick, MAX_STR_LEN))
            {
                continue;
            }
            /* Thread-safe call to find_NICK */
            nick_t *msgtarget = server_find_NICK(ctx, cc->nick);
            chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
            sds prefix = sdscatprintf(sdsempty(), ":%s!%s@%s",
                                      s->info.nick,
                                      s->info.username,
                                      client_hostname);

            if (server_reply_privmsg(ctx, prefix, cmdtokens, argc,
                                     msgtarget->client_socket) == MSG_ERROR)
            {
                return CHIRC_ERROR;
            }

            sdsfree(prefix);
        }
        pthread_mutex_unlock(&ctx->channels_lock);

        return CHIRC_OK;
    }

    nick_t *msgtarget = server_find_NICK(ctx, cmdtokens[1]);

    if (msgtarget == NULL) // Nickname not exist
    {
        chilog(ERROR, "ERR_NOSUCHNICK\n");
        return CHIRC_ERROR;
    }

    sds msg_prefix = sdscatprintf(sdsempty(), ":%s!%s@%s",
                                  s->info.nick,
                                  s->info.username,
                                  client_hostname); // reply msg prefix

    if (server_reply_privmsg(ctx, msg_prefix, cmdtokens, argc,
                             msgtarget->client_socket) == MSG_ERROR)
    {
        return CHIRC_ERROR;
    }

    return CHIRC_OK;
}


int handle_PING(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;

    sds nick = sdsempty();
    client_t *s = server_find_USER(ctx, client_socket);

    if (s == NULL)
    {
        reply_error(cmdtokens, ERR_NOSUCHSERVER, conn, ctx);
        return CHIRC_ERROR;
    }
    else
    {
        nick = sdscpy(nick, s->info.nick);
    }

    sds prefix = sdscatsds(sdsnew(":"), server_hostname); // Reply msg prefix

    if (server_reply_ping(ctx, prefix, server_hostname, client_socket) == MSG_ERROR)
    {
        return CHIRC_ERROR;
    }

    sdsfree(prefix);

    return CHIRC_OK;
}


int handle_PONG(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    return CHIRC_OK;
}


int handle_LUSERS(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;
    sds nick = sdsempty();

    /* Thread-safe call to find_USER */
    client_t *s = server_find_USER(ctx, client_socket);

    if (s == NULL)
    {
        nick = "*";
    }
    else
    {
        nick = s->info.nick;
    }

    if (server_reply_lusers(ctx, nick, conn) == MSG_ERROR)
    {
        return CHIRC_ERROR;
    }

    return CHIRC_OK;
}


int handle_WHOIS(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;

    client_t *s = server_find_USER(ctx, client_socket);

    if (s == NULL) // Not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, conn, ctx);

        return CHIRC_ERROR;
    }
    if (s->info.state == USER_MISSING || s->info.state == NICK_MISSING) // not registered
    {
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, conn, ctx);

        return CHIRC_ERROR;
    }

    if (argc - 1 < WHOIS_PARAMETER_NUM)
    {
        return CHIRC_ERROR;
    }

    nick_t *msgtarget = server_find_NICK(ctx, cmdtokens[1]);

    if (msgtarget == NULL) // Nickname not exist
    {
        chilog(ERROR, "ERR_NOSUCHNICK\n");
        reply_error(cmdtokens, ERR_NOSUCHNICK, conn, ctx);
        return CHIRC_ERROR;
    }
    client_t *starget = server_find_USER(ctx, msgtarget->client_socket);

    sds msg_prefix = sdscatsds(sdsnew(":"), server_hostname);

    /* RPL_WHOISUSER */
    if (server_reply_whois(ctx, msg_prefix, RPL_WHOISUSER, cmdtokens, conn, s->info.nick, starget) == MSG_ERROR)
        {
            return CHIRC_ERROR;
        }

    /* RPL_WHOISSERVER */
    if (server_reply_whois(ctx, msg_prefix, RPL_WHOISSERVER, cmdtokens, conn, s->info.nick, starget) == MSG_ERROR)
    {
        return CHIRC_ERROR;
    }

    /* RPL_ENDOFWHOIS */
    if (server_reply_whois(ctx, msg_prefix, RPL_ENDOFWHOIS, cmdtokens, conn, s->info.nick, starget) == MSG_ERROR)
    {
        return CHIRC_ERROR;
    }

    return CHIRC_OK;
}


int handle_LIST(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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

    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;
    channel_t *channel = NULL;
    sds channel_name = sdsempty();
    int num_clients = 0;

    client_t *s = server_find_USER(ctx, client_socket);

    if (s == NULL) // Not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, conn, ctx);

        return CHIRC_ERROR;
    }
    if (s->info.state != REGISTERED) // Not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, conn, ctx);

        return CHIRC_ERROR;
    }

    sds msg_all = sdsempty();
    sds msg_prefix = sdscatsds(sdsnew(":"), server_hostname);
    if (argc == 1)
    {
        pthread_mutex_lock(&ctx->channels_lock);
        for (channel = ctx->channels_hashtable; channel != NULL; channel = channel->hh.next)
        {
            channel_name = channel->channel_name;
            num_clients = HASH_COUNT(channel->channel_clients);

            msg_all = sdscatsds(msg_all, server_reply_list(ctx, msg_prefix,
                                                           RPL_LIST,
                                                           s->info.nick,
                                                           channel_name,
                                                           sdsfromlonglong(num_clients)));
        }
        pthread_mutex_unlock(&ctx->channels_lock);
    }
    else if (argc == 2)
    {
        channel_t *channel = server_find_CHANNEL(ctx, cmdtokens[1]);

        pthread_mutex_lock(&ctx->channels_lock);
        num_clients = HASH_COUNT(channel->channel_clients);
        pthread_mutex_unlock(&ctx->channels_lock);

        msg_all = sdscatsds(msg_all, server_reply_list(ctx, msg_prefix,
                                                       RPL_LIST,
                                                       s->info.nick,
                                                       cmdtokens[1],
                                                       sdsfromlonglong(num_clients)));
    }

    send_msg(client_socket, ctx, msg_all);

    sdsfree(msg_all);

    if (server_reply_listend(ctx, msg_prefix, RPL_LISTEND, s->info.nick, client_socket) == MSG_ERROR)
    {
        return CHIRC_ERROR;
    }

    return CHIRC_OK;
}


int handle_MODE(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;

    if (argc - 1 < MODE_PARAMETER_NUM)
    {
        return CHIRC_ERROR;
    }

    char *channel_name = cmdtokens[1];
    char *mode = cmdtokens[2];
    char *nick = cmdtokens[3];

    client_t *client = server_find_USER(ctx, client_socket);
    channel_t *channel = server_find_CHANNEL(ctx, channel_name);

    if (channel == NULL)
    {
        /* ERR_NOSUCHCHANNEL */
        chilog(ERROR, "ERR_NOSUCHCHANNEL");
        reply_error(cmdtokens, ERR_NOSUCHCHANNEL, conn, ctx);

        return CHIRC_ERROR;
    }

    if (strncmp(mode, "+o", MAX_STR_LEN) && strncmp(mode, "-o", MAX_STR_LEN))
    {
        /* UNKNOWNMODE */
        chilog(ERROR, "UNKNOWNMODE");
        reply_error(cmdtokens, ERR_UNKNOWNMODE, conn, ctx);

        return CHIRC_ERROR;
    }

    channel_client *chan = server_find_CHANNEL_CLIENT(ctx, channel, nick);

    if (chan == NULL)
    {
        /* ERR_USERNOTINCHANNEL */
        chilog(ERROR, "ERR_USERNOTINCHANNEL");
        reply_error(cmdtokens, ERR_USERNOTINCHANNEL, conn, ctx);

        return CHIRC_ERROR;
    }

    channel_client *owner = server_find_CHANNEL_CLIENT(ctx, channel, client->info.nick);

    if (owner == NULL || owner->mode == NULL ||
        (strncmp(owner->mode, "o", MAX_STR_LEN) != 0 &&
         (client->info.is_irc_operator == false)))
    {
        /* ERR_CHANOPRIVSNEEDED */
        chilog(ERROR, "ERR_CHANOPRIVSNEEDED");
        reply_error(cmdtokens, ERR_CHANOPRIVSNEEDED, conn, ctx);

        return CHIRC_ERROR;
    }

    if (strncmp(mode, "+o", MAX_STR_LEN) == 0)
    {
        chan->mode = sdsnew("o");
    }
    else
    {
        chan->mode = sdsnew("-o");
    }

    sds msg_prefix = sdscatprintf(sdsempty(), ":%s!%s@%s",
                                  client->info.nick,
                                  client->info.username,
                                  client_hostname);

    pthread_mutex_lock(&ctx->channels_lock);
    /* Send msg to each client in the channel */
    for (chan = channel->channel_clients; chan != NULL; chan = chan->hh.next)
    {
        nick_t *msgtarget = server_find_NICK(ctx, chan->nick);
        // chilog(TRACE, "%s relay to: %s", client->info.nick, chan->nick);

        if (server_reply_mode(ctx, msg_prefix, cmdtokens, msgtarget->client_socket) == MSG_ERROR)
        {
            return CHIRC_ERROR;
        }
    }
    pthread_mutex_unlock(&ctx->channels_lock);

    sdsfree(msg_prefix);

    return CHIRC_OK;
}


int handle_OPER(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;

    client_t *client = server_find_USER(ctx, client_socket);

    if (argc - 1 < OPER_PARAMETER_NUM)
    {
        /* ERR_NEEDMOREPARAMS */
        reply_error(cmdtokens, ERR_NEEDMOREPARAMS, conn, ctx);

        return CHIRC_ERROR;
    }

    if (strncmp(cmdtokens[2], ctx->password, MAX_STR_LEN))
    {
        /* ERR_PASSWDMISMATCH */
        reply_error(cmdtokens, ERR_PASSWDMISMATCH, conn, ctx);

        return CHIRC_ERROR;
    }

    // Find active irc_operator
    irc_oper_t *irc_operator_value = server_find_OPER(ctx, cmdtokens[1]);

    if (irc_operator_value == NULL)
    {
        irc_operator_value = (irc_oper_t *)malloc(sizeof(irc_oper_t));
        irc_operator_value->nick = sdsempty();
        irc_operator_value->mode = sdsempty();
        irc_operator_value->nick = sdscpy(irc_operator_value->nick, cmdtokens[1]);
        server_add_OPER(ctx, irc_operator_value);
    }

    irc_operator_value->mode = sdsnew("o");

    client->info.is_irc_operator = true;
    sds prefix = sdscatprintf(sdsempty(), ":%s!%s@%s",
                              client->info.nick,
                              client->info.username,
                              client_hostname);
    if (server_reply_oper(ctx, prefix, RPL_YOUREOPER, cmdtokens, client_socket) == MSG_ERROR)
    {
        return CHIRC_ERROR;
    }

    return CHIRC_OK;
}


int handle_PART(server_ctx *ctx, sds *cmdtokens, int argc, conn_info_t *conn)
{
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
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;

    client_t *s = server_find_USER(ctx, client_socket);

    if (s == NULL) // Not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, conn, ctx);

        return CHIRC_ERROR;
    }
    if (s->info.state != REGISTERED) // Not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, conn, ctx);

        return CHIRC_ERROR;
    }

    /* ERR_NEEDMOREPARAMS */
    if (argc - 1 < PART_PARAMETER_NUM)
    {
        reply_error(cmdtokens, ERR_NEEDMOREPARAMS, conn, ctx);

        return CHIRC_ERROR;
    }

    channel_t *c = server_find_CHANNEL(ctx, cmdtokens[1]);

    if (c == NULL) // Channel not exist
    {
        chilog(ERROR, "ERR_NOSUCHCHANNEL\n");
        reply_error(cmdtokens, ERR_NOSUCHCHANNEL, conn, ctx);

        return CHIRC_ERROR;
    }

    channel_client *cc = server_find_CHANNEL_CLIENT(ctx, c, s->info.nick);

    if (cc == NULL) // Client not in the channel
    {
        chilog(ERROR, "ERR_NOTONCHANNEL\n");
        reply_error(cmdtokens, ERR_NOTONCHANNEL, conn, ctx);

        return CHIRC_ERROR;
    }

    /* Leave the channel */
    sds prefix = sdscatprintf(sdsempty(), ":%s!%s@%s",
                              s->info.nick,
                              s->info.username,
                              client_hostname);

    /* Send msg to each client in the channel */
    pthread_mutex_lock(&ctx->channels_lock);
    for (cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
    {
        nick_t *msgtarget = server_find_NICK(ctx, cc->nick);

        if (server_reply_part(ctx, prefix, cmdtokens, c->channel_name,
                              argc, msgtarget->client_socket) == MSG_ERROR)
        {
            return CHIRC_ERROR;
        }
    }
    sdsfree(prefix);
    remove_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
    if (HASH_COUNT(c->channel_clients) <= 0)
    {
        remove_CHANNEL(c->channel_name, &ctx->channels_hashtable);
    }
    pthread_mutex_unlock(&ctx->channels_lock);

    return CHIRC_OK;
}