#include "handlers.h"
#include "msg.h"
#include "log.h"
#include "reply.h"
#include "../lib/uthash.h"
#include "../lib/sds/sds.h"

#include "channels.h"
#include "sendall.h"

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

int handle_request(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    int j, rc = 0;
    int len;                                 // length of msg to send
    sds prefix = sdsnew(":bar.example.com"); // reply msg prefix
    char *code;                              // the three-difit code of reply msg
    int num_handlers = sizeof(handlers) / sizeof(struct handler_entry);
    for (j = 0; j < num_handlers; j++)
    {
        if (!strcmp(handlers[j].name, cmdtokens[0]))
        {

            // rc = handlers[j].func(ctx, client_socket, cmdtokens, argc, client_hostname);
            rc = handlers[j].func(ctx, client_socket, cmdtokens, argc, server_hostname);
            break;
        }
    }
    client_t **client_hashtable = &ctx->client_hashtable;
    pthread_mutex_lock(&ctx->clients_lock);
    client_t *s = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);
    if (s == NULL)
    {
        chilog(INFO, "Registration Failed");
        return -1;
    }

    if (s->info.state == REGISTERED && strcmp(cmdtokens[0],"PONG") != 0 && strcmp(cmdtokens[0],"PRIVMSG") != 0 && strcmp(cmdtokens[0],"PART") != 0
        && strcmp(cmdtokens[0],"NOTICE") != 0 && strcmp(cmdtokens[0],"WHOIS") != 0 && strcmp(cmdtokens[0],"JOIN") != 0)
    {
        if (j == num_handlers) // unknown command
        {
            reply_error(cmdtokens, ERR_UNKNOWNCOMMAND, client_socket, client_hashtable);
            chilog(ERROR, "Unknown handler code %s\n", cmdtokens[0]);
            return -1;
        }
        if (strcmp(cmdtokens[0], "USER") == 0 || strcmp(cmdtokens[0], "NICK") == 0)
        {
            // code = RPL_WELCOME; // 001   WHERE TO PUT THE CODE?
            sds msg = sdscatprintf(sdsempty(), "%s %s %s :Welcome to the Internet Relay Network %s!%s@foo.example.com\r\n", prefix, RPL_WELCOME, s->info.nick, s->info.nick, s->info.username);
        
            len = sdslen(msg);
            if (sendall(client_socket, msg, &len) == -1) {
                perror("sendall");
                printf("We only sent %d bytes because of the error!\n", len);
            } 
            chilog(INFO, "send msg: %s", msg);
            sdsfree(msg);
        }

        /* RPL_YOURHOST */
        sds host_msg = sdscatprintf(sdsempty(), "%s %s %s :Your host is %s, running version %s\r\n", prefix, RPL_YOURHOST, s->info.nick, "bar.example.com", VERSION);

        len = sdslen(host_msg);
        if (sendall(client_socket, host_msg, &len) == -1) {
            perror("sendall");
            printf("We only sent %d bytes because of the error!\n", len);
        } 
        chilog(INFO, "send msg: %s", host_msg);

        time_t t = time(NULL);
        struct tm tm = *localtime(&t);

        /* RPL_CREATED */
        sds create_msg = sdscatprintf(sdsempty(), "%s %s %s :This server was created %d-%02d-%02d %02d:%02d:%02d\r\n", prefix, RPL_CREATED, s->info.nick,
                                      tm.tm_year + 1900,
                                      tm.tm_mon + 1,
                                      tm.tm_mday,
                                      tm.tm_hour,
                                      tm.tm_min,
                                      tm.tm_sec);

        len = sdslen(create_msg);
        if (sendall(client_socket, create_msg, &len) == -1) {
            perror("sendall");
            printf("We only sent %d bytes because of the error!\n", len);
        } 
        chilog(INFO, "send msg: %s", create_msg);


        /*RPL_MYINFO */
        sds info_msg = sdscatprintf(sdsempty(), "%s %s %s %s %s %s %s\r\n", prefix, RPL_MYINFO, s->info.nick, "bar.example.com", VERSION, "ao", "mtov");

        len = sdslen(info_msg);
        if (sendall(client_socket, info_msg, &len) == -1) {
            perror("sendall");
            printf("We only sent %d bytes because of the error!\n", len);
        } 
        chilog(INFO, "send msg: %s", info_msg);

        handle_LUSERS(ctx, client_socket, cmdtokens, argc, server_hostname);

        /* ERR_MOTD */
        sds nomotd_msg = sdscatprintf(sdsempty(), "%s %s %s :MOTD File is missing\r\n", prefix, ERR_NOMOTD, s->info.nick);

        len = sdslen(nomotd_msg);
        if (sendall(client_socket, nomotd_msg, &len) == -1) {
            perror("sendall");
            printf("We only sent %d bytes because of the error!\n", len);
        } 
        chilog(INFO, "send msg: %s", nomotd_msg);

        sdsfree(host_msg);
        sdsfree(create_msg);
        sdsfree(info_msg);
        sdsfree(nomotd_msg);
    }
    return 1;
}

int handle_NICK(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    client_t **client_hashtable = &ctx->client_hashtable;
    nick_t **nicks_hashtable = &ctx->nicks_hashtable;
    /* implement the ERR_NONICKNAMEGIVEN */
    if (argc - 1 < NICK_PARAMETER_NUM)
    {
        chilog(ERROR, "ERR_NONICKNAMEGIVEN\n");
        reply_error(cmdtokens, ERR_NONICKNAMEGIVEN, client_socket, client_hashtable);
        return -1;
    }

    sds nickname = cmdtokens[1];
    pthread_mutex_lock(&ctx->nicks_lock);
    nick_t *find = find_NICK(nickname, nicks_hashtable);
    pthread_mutex_unlock(&ctx->nicks_lock);
    if (find != NULL)
    {
        /* Nick is already in use */
        // ERR_NICKNAMEINUSE
        chilog(ERROR, "ERR_NICKNAMEINUSE\n");
        reply_error(cmdtokens, ERR_NICKNAMEINUSE, client_socket, client_hashtable);
        // reply_registration
        return -1;
    }
    pthread_mutex_lock(&ctx->lock);
    ctx->num_connections++;
    pthread_mutex_unlock(&ctx->lock);

    pthread_mutex_lock(&ctx->clients_lock);
    client_t *s = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);
    if (s == NULL)
    {
        // chilog(INFO, "First time user %s\n", nickname);
        s = malloc(sizeof(client_t));
        s->info.nick = sdsempty();
        s->info.realname = sdsempty();
        s->info.username = sdsempty();
        s->info.state = NOT_REGISTERED;
        s->socket = client_socket;
    }
    else
    {
        chilog(INFO, "NOT First time user\n");
    }

    if (argc - 1 < NICK_PARAMETER_NUM)
    {
        chilog(ERROR, "ERR_NONICKNAMEGIVEN\n");
        reply_error(cmdtokens, ERR_NONICKNAMEGIVEN, client_socket, client_hashtable);
        return -1;
    }
    if (s->info.state == NICK_MISSING)
    {
        s->info.state = REGISTERED;
        s->info.nick = sdscpy(s->info.nick, cmdtokens[1]);
        chilog(INFO, "Registration Success\n");
        // Add NICK to nick_hashtable
        pthread_mutex_lock(&ctx->nicks_lock);
        add_NICK(s->info.nick, s->socket, nicks_hashtable);
        pthread_mutex_unlock(&ctx->nicks_lock);

        //reply_registration(ctx, cmdtokens, argc, client_socket, client_hashtable, server_hostname);
        return 3;
    }
    else if (s->info.state == REGISTERED)
    {
        chilog(INFO, "Update NICK\n");
        /* reply to self */
        chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
        sds prefix = sdscatprintf(sdsempty(), ":%s!%s@foo.example.com", s->info.nick, s->info.nick);
        chirc_message_construct(msg, prefix, "NICK");
        sds param = sdsjoinsds(cmdtokens + 1, argc - 1, " ", 1);
        param = sdscat(param, "\r\n");
        chirc_message_add_parameter(msg, param, true);
        sds host_msg;
        chirc_message_to_string(msg, &host_msg);

        int len = sdslen(host_msg);
        if (sendall(client_socket, host_msg, &len) == -1) {
            perror("sendall");
            printf("We only sent %d bytes because of the error!\n", len);
        }
        sdsfree(prefix);
        sdsfree(host_msg);
        sdsfree(param);
        chirc_message_destroy(msg);

        // reply nick update to channels
        channel_t **channel_hashtable = &ctx->channels_hashtable;
        channel_t *c;
        for (c = ctx->channels_hashtable; c != NULL; c = c->hh.next)
        {
            channel_client *cc = NULL;
            pthread_mutex_lock(&ctx->channels_lock);
            cc = find_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
            pthread_mutex_unlock(&ctx->channels_lock);
            if (cc == NULL) // not in the channel
                continue;
            for (cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
            {
                // reply to the clients in the channel

                /* do not send msg to self more than once*/
                if (strcmp(cc->nick, s->info.nick) == 0)
                {
                    continue;
                }
                pthread_mutex_lock(&ctx->nicks_lock);
                nick_t *msgtarget = find_NICK(cc->nick, &ctx->nicks_hashtable);
                pthread_mutex_unlock(&ctx->nicks_lock);
                msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
                prefix = sdscatprintf(sdsempty(), ":%s!%s@foo.example.com", s->info.nick, s->info.nick);
                chirc_message_construct(msg, prefix, "NICK");
                param = sdsjoinsds(cmdtokens + 1, argc - 1, " ", 1);
                param = sdscat(param, "\r\n");
                chirc_message_add_parameter(msg, param, true);
                host_msg;
                chirc_message_to_string(msg, &host_msg);

                len = sdslen(host_msg);
                if (sendall(msgtarget->client_socket, host_msg, &len) == -1) {
                    perror("sendall");
                    printf("We only sent %d bytes because of the error!\n", len);
                }
                sdsfree(prefix);
                sdsfree(host_msg);
                sdsfree(param);
                chirc_message_destroy(msg);
            } 
        }
        
        s->info.nick = sdscpy(s->info.nick, cmdtokens[1]);
        // Update nick hashtable
        pthread_mutex_lock(&ctx->nicks_lock);
        remove_NICK(s->info.nick, nicks_hashtable);
        add_NICK(s->info.nick, client_socket, nicks_hashtable);
        pthread_mutex_unlock(&ctx->nicks_lock);

        return 3;
    }
    else if (s->info.state == NOT_REGISTERED)
    {
        s->info.state = USER_MISSING;
        s->info.nick = sdscpy(s->info.nick, cmdtokens[1]);
        chilog(INFO, "NOT_REGISTERED\n");
        pthread_mutex_lock(&ctx->clients_lock);
        add_USER(s, client_socket, client_hashtable);
        pthread_mutex_unlock(&ctx->clients_lock);
        chilog(INFO, "NICK Recieved\n");

        return 1;
    }
    s->info.nick = sdscpy(s->info.nick, cmdtokens[1]);

    return 1;
}

int handle_USER(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    client_t **client_hashtable = &ctx->client_hashtable;
    nick_t **nicks_hashtable = &ctx->nicks_hashtable;
    /*implement the ERR_NEEDMOREPARAMS */
    if (argc - 1 < USER_PARAMETER_NUM)
    {
        reply_error(cmdtokens, ERR_NEEDMOREPARAMS, client_socket, client_hashtable);
        return 0;
    }
    chilog(INFO, "USER: Number of Parameters %d\n", argc);

    pthread_mutex_lock(&ctx->clients_lock);
    client_t *s = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);
    if (s != NULL && s->info.state == REGISTERED)
    {
        /* USER is already registered */
        // ERR_ALREADYREGISTRED
        //reply_error(cmdtokens, ERR_ALREADYREGISTRED, client_socket, client_hashtable);
        return 3;
    }

    if (s == NULL)
    {
        s = malloc(sizeof(client_t));
        chilog(INFO, "USER_NOT_FOUND\n");
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
    chilog(INFO, "handle_USER %s\n", s->info.username);
    if ((int)sdslen(s->info.username) == 0)
    {
        chilog(ERROR, "Not handled USER message sussessfully.");
        return 0;
    }
    if (s->info.state == USER_MISSING)
    {
        s->info.state = REGISTERED;
        chilog(INFO, "Registration Success %s\n", cmdtokens[1]);
        pthread_mutex_lock(&ctx->nicks_lock);
        add_NICK(s->info.nick, client_socket, nicks_hashtable);
        pthread_mutex_unlock(&ctx->nicks_lock);
        //reply_registration(ctx, cmdtokens, argc, client_socket, client_hashtable, server_hostname);
        return 3;
    }
    else if (s->info.state == NOT_REGISTERED)
    {
        s->info.state = NICK_MISSING;
        pthread_mutex_lock(&ctx->clients_lock);
        add_USER(s, client_socket, client_hashtable);
        pthread_mutex_unlock(&ctx->clients_lock);
        return 2;
    }
    return 2;
}

int handle_QUIT(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    chilog(INFO, "Update QUIT\n");
    client_t **client_hashtable = &ctx->client_hashtable;
    pthread_mutex_lock(&ctx->clients_lock);
    client_t *s = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);
    if (s == NULL) // not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, client_socket, client_hashtable);
        return -1;
    }
    if (s->info.state != REGISTERED) // not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, client_socket, client_hashtable);
        return -1;
    }
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
    sds reply_msg = sdscatprintf(sdsempty(), "ERROR :Closing Link: %s (%s)\r\n", CLIENTHOST, msg);                       
    int len;
    len = sdslen(reply_msg);
    if (sendall(client_socket, reply_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    } 

    // reply nick update to channels
    channel_t **channel_hashtable = &ctx->channels_hashtable;
    channel_t *c;
    for (c = ctx->channels_hashtable; c != NULL; c = c->hh.next)
    {
        channel_client *cc = NULL;
        pthread_mutex_lock(&ctx->channels_lock);
        cc = find_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
        pthread_mutex_unlock(&ctx->channels_lock);
        if (cc == NULL) // not in the channel
            continue;
        for (cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
        {
            // reply to the clients in the channel

            /* do not send msg to self more than once*/
            if (strcmp(cc->nick, s->info.nick) == 0)
            {
                continue;
            }
            pthread_mutex_lock(&ctx->nicks_lock);
            nick_t *msgtarget = find_NICK(cc->nick, &ctx->nicks_hashtable);
            pthread_mutex_unlock(&ctx->nicks_lock);
            chirc_message_t *quit_msg = (chirc_message_t *) malloc(sizeof(chirc_message_t));
            sds prefix = sdscatprintf(sdsempty(), ":%s!%s@foo.example.com", s->info.nick, s->info.nick);
            chirc_message_construct(quit_msg, prefix, "QUIT");
            sds param;
            if (argc > 1) {
                param = sdsjoinsds(cmdtokens + 1, argc - 1, " ", 1);
                if (param[0] == ':')
                {
                    sdsrange(param, 1, sdslen(param));
                }
                param = sdscat(param, "\r\n");
            } else {
                param = sdscatprintf(sdsempty(), "Client Quit\r\n");
            }
            chirc_message_add_parameter(quit_msg, param, true);
            sds host_msg;
            chirc_message_to_string(quit_msg, &host_msg);
            len = sdslen(host_msg);
            if (sendall(msgtarget->client_socket, host_msg, &len) == -1) {
                perror("sendall");
                printf("We only sent %d bytes because of the error!\n", len);
            }
            sdsfree(prefix);
            sdsfree(host_msg);
            sdsfree(param);
            chirc_message_destroy(quit_msg);
        } 
        remove_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
        if (HASH_COUNT(c->channel_clients) <= 0)
        {
            remove_CHANNEL(c->channel_name, channel_hashtable);
        }
    }
    chilog(INFO, "!!!!!\n");
    close_socket(ctx, client_socket);
    pthread_exit(NULL);
    return 1;
}

int handle_JOIN(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    client_t **client_hashtable = &ctx->client_hashtable;
    channel_t **channel_hashtable = &ctx->channels_hashtable;
    sds channel_name = cmdtokens[1];
    pthread_mutex_lock(&ctx->clients_lock);
    client_t *s = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);
    if (s == NULL) // not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, client_socket, client_hashtable);
        return -1;
    }
    if (s->info.state != REGISTERED) // not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, client_socket, client_hashtable);
        return -1;
    }
    /* ERR_NONICKNAMEGIVEN */
    if (argc - 1 < JOIN_PARAMETER_NUM)
    {
        reply_error(cmdtokens, ERR_NEEDMOREPARAMS, client_socket, client_hashtable);
        return -1;
    }
    pthread_mutex_lock(&ctx->channels_lock);
    channel_t *c = find_CHANNEL(channel_name, channel_hashtable);
    pthread_mutex_unlock(&ctx->channels_lock);
    bool flag = 1; //if flag == 1, channel exists, else channel is newly created.
    if (c == NULL)
    {
        // channel not exist
        // chilog(INFO, "channel_hashtable size %d\n", HASH_COUNT(*channel_hashtable));
        pthread_mutex_lock(&ctx->channels_lock);
        c = add_CHANNEL(channel_name, channel_hashtable);
        pthread_mutex_unlock(&ctx->channels_lock);
        chilog(INFO, "add Channel %s\n", channel_name);
        flag = 0;
    }

    channel_client *cc = NULL;
    pthread_mutex_lock(&ctx->channels_lock);
    cc = find_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
    pthread_mutex_unlock(&ctx->channels_lock);
    if (cc != NULL)
    {
        // client already in the channel
        return 1;
    }

    /* add client to channel */
    pthread_mutex_lock(&ctx->channels_lock);
    add_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
    if (flag != 0) {
        cc = find_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
        cc->mode = "+o";
    }
    pthread_mutex_unlock(&ctx->channels_lock);
    chilog(INFO, "handle_JOIN %s\n", s->info.nick);

    /* RPL_NAMREPLY */
    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
    char *prefix = ":bar.example.com";
    char *cmd = RPL_NAMREPLY;
    chirc_message_construct(msg, prefix, cmd);
    chirc_message_add_parameter(msg, s->info.nick, false);
    chirc_message_add_parameter(msg, "=", false);
    sds param = sdsempty();
    sdscatsds(param, channel_name);
    chirc_message_add_parameter(msg, param, false);

    /* add name list as param */
    param = sdscpy(param, "@");
    for (cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
    {
        param = sdscatsds(param, cc->nick);
        if (cc->hh.next)
        {
            param = sdscat(param, " ");
        }

        /* send JOIN msg to each client in the channel */
        pthread_mutex_lock(&ctx->nicks_lock);
        nick_t *msgtarget = find_NICK(cc->nick, &ctx->nicks_hashtable);
        pthread_mutex_unlock(&ctx->nicks_lock);
        chirc_message_t *join_msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
        sds join_prefix = sdscatprintf(sdsempty(), ":%s!%s@foo.example.com", s->info.nick, s->info.nick);
        chirc_message_construct(join_msg, join_prefix, "JOIN");
        sds join_param = sdsnew(channel_name);
        join_param = sdscat(join_param, "\r\n");
        chirc_message_add_parameter(join_msg, join_param, false);
        sds host_join_msg;
        chirc_message_to_string(join_msg, &host_join_msg);

        int len;
        len = sdslen(host_join_msg);
        if (sendall(msgtarget->client_socket, host_join_msg, &len) == -1) {
            perror("sendall");
            printf("We only sent %d bytes because of the error!\n", len);
        } 


        sdsfree(join_prefix);
        sdsfree(host_join_msg);
        sdsfree(join_param);
        chirc_message_destroy(join_msg);
    }
    
    param = sdscat(param, "\r\n");
    chirc_message_add_parameter(msg, param, true);

    sds host_msg;
    chirc_message_to_string(msg, &host_msg);

    int len;
    len = sdslen(host_msg);
    if (sendall(client_socket, host_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    } 
    chirc_message_destroy(msg);

    /* RPL_ENDOFNAMES */
    msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
    chirc_message_construct(msg, prefix, "366");
    chirc_message_add_parameter(msg, s->info.nick, false);
    param = sdsempty();
    sdscatsds(param, channel_name);
    chirc_message_add_parameter(msg, param, false);
    param = sdscpy(param, "End of NAMES list\r\n");
    chirc_message_add_parameter(msg, param, true);
    chirc_message_to_string(msg, &host_msg);

    len = sdslen(host_msg);
    if (sendall(client_socket, host_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
    chirc_message_destroy(msg);
    sdsfree(param);
    sdsfree(host_msg);

    return 1;
}

int handle_PRIVMSG(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    int len;
    client_t **client_hashtable = &ctx->client_hashtable;
    pthread_mutex_lock(&ctx->clients_lock);
    client_t *s = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);
    char *prefix = ":bar.example.com";
    if (s == NULL) // not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, client_socket, client_hashtable);
        return -1;
    }
    if (s->info.state != REGISTERED) // not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, client_socket, client_hashtable);
        return -1;
    }
    
    if (argc - 1 < PRIVMSG_PARAMETER_NUM)
    {
        if (argc == 1)
        {
            chilog(ERROR, "ERR_NORECIPIENT\n");
            reply_error(cmdtokens, ERR_NORECIPIENT, client_socket, client_hashtable);
            return -1;
        }
        else if (argc == 2)
        {
            chilog(ERROR, "ERR_NOTEXTTOSEND\n");
            reply_error(cmdtokens, ERR_NOTEXTTOSEND, client_socket, client_hashtable);
            return -1;
        }
    }

    /* PRIVMSG to channels */
    if (cmdtokens[1][0] == '#')
    {
        channel_t **channel_hashtable = &ctx->channels_hashtable;
        pthread_mutex_lock(&ctx->channels_lock);
        channel_t *c = find_CHANNEL(cmdtokens[1], channel_hashtable);
        pthread_mutex_unlock(&ctx->channels_lock);
        if (c == NULL) // channel not exist
        {
            chilog(ERROR, "ERR_NOSUCHCHANNEL\n");
            reply_error(cmdtokens, ERR_NOSUCHNICK, client_socket, client_hashtable);
            return -1;
        }

        channel_client *cc = NULL;
        pthread_mutex_lock(&ctx->channels_lock);
        cc = find_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
        pthread_mutex_unlock(&ctx->channels_lock);
        if (cc == NULL) // client not in the channel
        {
            chilog(ERROR, "ERR_CANNOTSENDTOCHAN\n");
            reply_error(cmdtokens, ERR_CANNOTSENDTOCHAN, client_socket, client_hashtable);
            return -1;
        }

        /* send msg to each client in the channel */
        for (cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
        {

            /* do not send msg to self */
            if (strcmp(cc->nick, s->info.nick) == 0)
            {
                continue;
            }
            pthread_mutex_lock(&ctx->nicks_lock);
            nick_t *msgtarget = find_NICK(cc->nick, &ctx->nicks_hashtable);
            pthread_mutex_unlock(&ctx->nicks_lock);
            chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
            sds prefix = sdscatprintf(sdsempty(), ":%s!%s@foo.example.com", s->info.nick, s->info.nick);
            chirc_message_construct(msg, prefix, "PRIVMSG");
            chirc_message_add_parameter(msg, c->channel_name, false);
            sds param = sdsjoinsds(cmdtokens + 2, argc - 2, " ", 1);
            if (param[0] == ':')
            {
                sdsrange(param, 1, sdslen(param));
            }
            param = sdscat(param, "\r\n");
            chirc_message_add_parameter(msg, param, true);
            sds host_msg;
            chirc_message_to_string(msg, &host_msg);

            len = sdslen(host_msg);
            if (sendall(msgtarget->client_socket, host_msg, &len) == -1) {
                perror("sendall");
                printf("We only sent %d bytes because of the error!\n", len);
            }


            sdsfree(prefix);
            sdsfree(host_msg);
            sdsfree(param);
            chirc_message_destroy(msg);
        }
        return 1;
    }

    nick_t **nick_hashtable = &ctx->nicks_hashtable;
    pthread_mutex_lock(&ctx->nicks_lock);
    nick_t *msgtarget = find_NICK(cmdtokens[1], nick_hashtable);
    pthread_mutex_unlock(&ctx->nicks_lock);

    if (msgtarget == NULL) // nickname not exist
    {
        chilog(ERROR, "ERR_NOSUCHNICK\n");
        reply_error(cmdtokens, ERR_NOSUCHNICK, client_socket, client_hashtable);
        return -1;
    }

    sds msg_prefix = sdscatprintf(sdsempty(), ":%s!%s@foo.example.com", s->info.nick, s->info.username); // reply msg prefix
    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
    chirc_message_construct(msg, msg_prefix, "PRIVMSG");
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
    chilog(INFO, "%s\n", host_msg);

    len = sdslen(host_msg);
    if (sendall(msgtarget->client_socket, host_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }

    return 1;
}

int handle_NOTICE(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    int len;
    client_t **client_hashtable = &ctx->client_hashtable;
    pthread_mutex_lock(&ctx->clients_lock);
    client_t *s = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);
    char *prefix = ":bar.example.com";
    if (s == NULL) // not registered
    {
        /* ERR_NOTREGISTERED */
        return -1;
    }
    if (s->info.state != REGISTERED) // not registered
    {
        return -1;
    }
    if (argc - 1 < PRIVMSG_PARAMETER_NUM)
    {
        return -1;
    }

    /* NOTICE to channels */
    if (cmdtokens[1][0] == '#')
    {
        channel_t **channel_hashtable = &ctx->channels_hashtable;
        pthread_mutex_lock(&ctx->channels_lock);
        channel_t *c = find_CHANNEL(cmdtokens[1], channel_hashtable);
        pthread_mutex_unlock(&ctx->channels_lock);
        if (c == NULL) // channel not exist
        {
            chilog(ERROR, "ERR_NOSUCHCHANNEL\n");
            return -1;
        }

        channel_client *cc = NULL;
        pthread_mutex_lock(&ctx->channels_lock);
        cc = find_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
        pthread_mutex_unlock(&ctx->channels_lock);
        if (cc == NULL) // client not in the channel
        {
            chilog(ERROR, "ERR_CANNOTSENDTOCHAN\n");
            return -1;
        }

        /* send msg to each client in the channel */
        for (cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
        {

            /* do not send msg to self */
            if (strcmp(cc->nick, s->info.nick) == 0)
            {
                continue;
            }
            pthread_mutex_lock(&ctx->nicks_lock);
            nick_t *msgtarget = find_NICK(cc->nick, &ctx->nicks_hashtable);
            pthread_mutex_unlock(&ctx->nicks_lock);
            chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
            sds prefix = sdscatprintf(sdsempty(), ":%s!%s@foo.example.com", s->info.nick, s->info.nick);
            chirc_message_construct(msg, prefix, "PRIVMSG");
            chirc_message_add_parameter(msg, c->channel_name, false);
            sds param = sdsjoinsds(cmdtokens + 2, argc - 2, " ", 1);
            if (param[0] == ':')
            {
                sdsrange(param, 1, sdslen(param));
            }
            param = sdscat(param, "\r\n");
            chirc_message_add_parameter(msg, param, true);
            sds host_msg;
            chirc_message_to_string(msg, &host_msg);

            len = sdslen(host_msg);
            if (sendall(msgtarget->client_socket, host_msg, &len) == -1) {
                perror("sendall");
                printf("We only sent %d bytes because of the error!\n", len);
            }

            sdsfree(prefix);
            sdsfree(host_msg);
            sdsfree(param);
            chirc_message_destroy(msg);
        }
        return 1;
    }

    nick_t **nick_hashtable = &ctx->nicks_hashtable;
    pthread_mutex_lock(&ctx->nicks_lock);
    nick_t *msgtarget = find_NICK(cmdtokens[1], nick_hashtable);
    pthread_mutex_unlock(&ctx->nicks_lock);

    if (msgtarget == NULL) // nickname not exist
    {
        chilog(ERROR, "ERR_NOSUCHNICK\n");
        return -1;
    }

    sds msg_prefix = sdscatprintf(sdsempty(), ":%s!%s@foo.example.com", s->info.nick, s->info.username); // reply msg prefix
    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
    chirc_message_construct(msg, msg_prefix, "NOTICE");
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
    chilog(INFO, "%s\n", host_msg);

    len = sdslen(host_msg);
    if (sendall(msgtarget->client_socket, host_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
    return 1;
}

int handle_PING(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    sds nick = sdsempty();
    client_t **client_hashtable = &ctx->client_hashtable;
    pthread_mutex_lock(&ctx->clients_lock);
    client_t *s = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);

    if (s == NULL)
    {
        reply_error(cmdtokens, ERR_NOSUCHSERVER, client_socket, client_hashtable);
    }
    else
    {
        nick = sdscpy(nick, s->info.nick);
    }
    sds prefix = sdsnew(":bar.example.com"); // reply msg prefix
    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
    chirc_message_construct(msg, prefix, "PONG");
    chirc_message_add_parameter(msg, "bar.example.com\r\n", true);
    sds host_msg;
    chirc_message_to_string(msg, &host_msg);
    
    int len = sdslen(host_msg);
    if (sendall(client_socket, host_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
    return 1;
}

int handle_PONG(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    return 1;
}

int handle_LUSERS(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    sds nick = sdsempty();
    client_t **client_hashtable = &ctx->client_hashtable;
    pthread_mutex_lock(&ctx->clients_lock);
    client_t *s = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);
    if (s == NULL)
    {
        nick = "*";
    }
    else
    {
        nick = s->info.nick;
    }
    sds prefix = sdsnew(":bar.example.com"); // reply msg prefix

    pthread_mutex_lock(&ctx->nicks_lock);
    int num_of_users = HASH_COUNT(ctx->nicks_hashtable);
    pthread_mutex_unlock(&ctx->nicks_lock);
    pthread_mutex_lock(&ctx->lock);
    int num_connections = ctx->num_connections;
    int num_of_unknown_connections = ctx->total_connections - num_connections;
    pthread_mutex_unlock(&ctx->lock);
    /* RPL_LUSERCLIENT */

    sds serclient_msg = sdscatprintf(sdsempty(), "%s %s %s :There are %d users and 0 services on 1 servers\r\n", prefix, RPL_LUSERCLIENT, nick, num_of_users);
    
    int len = sdslen(serclient_msg);
    if (sendall(client_socket, serclient_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }

    /* RPL_LUSEROP */
    irc_oper_t **irc_operators_hashtable = &ctx->irc_operators_hashtable;
    pthread_mutex_lock(&ctx->operators_lock);
    int num_of_irc_operator = HASH_COUNT(ctx->irc_operators_hashtable);
    pthread_mutex_unlock(&ctx->operators_lock);
    sds serop_msg = sdscatprintf(sdsempty(), "%s %s %s %d :operator(s) online\r\n", prefix, RPL_LUSEROP, nick, num_of_irc_operator);
    
    len = sdslen(serop_msg);
    if (sendall(client_socket, serop_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }

    

    /* RPL_LUSERUNKNOWN */
    sds serunknown_msg = sdscatprintf(sdsempty(), "%s %s %s %d :unknown connection(s)\r\n", prefix, RPL_LUSERUNKNOWN, nick, num_of_unknown_connections);
    
    len = sdslen(serunknown_msg);
    if (sendall(client_socket, serunknown_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }

    /* RPL_LUSERCHANNELS */
    channel_t **channel_hashtable = &ctx->channels_hashtable;
    pthread_mutex_lock(&ctx->channels_lock);
    int num_of_channels = HASH_COUNT(ctx->channels_hashtable);
    pthread_mutex_unlock(&ctx->channels_lock);
    sds serchannel_msg = sdscatprintf(sdsempty(), "%s %s %s %d :channels formed\r\n", prefix, RPL_LUSERCHANNELS, nick, num_of_channels);
    
    len = sdslen(serchannel_msg);
    if (sendall(client_socket, serchannel_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }

    /* RPL_LUSERME */
    sds serme_msg = sdscatprintf(sdsempty(), "%s %s %s :I have %d clients and 1 servers\r\n", prefix, RPL_LUSERME, nick, num_connections);
    
    len = sdslen(serme_msg);
    if (sendall(client_socket, serme_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }

    sdsfree(serclient_msg);
    sdsfree(serop_msg);
    sdsfree(serunknown_msg);
    sdsfree(serchannel_msg);
    sdsfree(serme_msg);

    return 0;
}

int handle_WHOIS(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    client_t **client_hashtable = &ctx->client_hashtable;
    pthread_mutex_lock(&ctx->clients_lock);
    client_t *s = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);
    char *prefix = ":bar.example.com";
    if (s == NULL) // not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, client_socket, client_hashtable);
        return -1;
    }
    if (s->info.state == USER_MISSING || s->info.state == NICK_MISSING) // not registered
    {
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, client_socket, client_hashtable);
        return -1;
    }

    if (argc - 1 < WHOIS_PARAMETER_NUM)
    {
        return -1;
    }

    nick_t **nick_hashtable = &ctx->nicks_hashtable;
    pthread_mutex_lock(&ctx->nicks_lock);
    nick_t *msgtarget = find_NICK(cmdtokens[1], nick_hashtable);
    pthread_mutex_unlock(&ctx->nicks_lock);

    if (msgtarget == NULL) // nickname not exist
    {
        chilog(ERROR, "ERR_NOSUCHNICK\n");
        reply_error(cmdtokens, ERR_NOSUCHNICK, client_socket, client_hashtable);
        return -1;
    }

    pthread_mutex_lock(&ctx->clients_lock);
    client_t *starget = find_USER(msgtarget->client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);

    sds msg_prefix = sdsnew(":bar.example.com"); // reply msg prefix
    /* RPL_WHOISUSER */
    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
    chirc_message_construct(msg, msg_prefix, "311");
    chirc_message_add_parameter(msg, s->info.nick, false);
    chirc_message_add_parameter(msg, cmdtokens[1], false);
    chirc_message_add_parameter(msg, starget->info.username, false);
    chirc_message_add_parameter(msg, "foo.example.com", false);
    chirc_message_add_parameter(msg, "*", false);
    sds param = sdsnew(starget->info.realname);
    param = sdscat(param, "\r\n");
    chirc_message_add_parameter(msg, param, true);
    sds host_msg_user;
    chirc_message_to_string(msg, &host_msg_user);
    chilog(INFO, "%s\n", host_msg_user);
    
    int len = sdslen(host_msg_user);
    if (sendall(client_socket, host_msg_user, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
    chirc_message_destroy(msg);

    /* RPL_WHOISSERVER */
    msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
    chirc_message_construct(msg, msg_prefix, "312");
    chirc_message_add_parameter(msg, s->info.nick, false);
    chirc_message_add_parameter(msg, cmdtokens[1], false);
    chirc_message_add_parameter(msg, "bar.example.com", false);
    chirc_message_add_parameter(msg, "*\r\n", true);
    sds host_msg_sever;
    chirc_message_to_string(msg, &host_msg_sever);
    chilog(INFO, "%s\n", host_msg_sever);
    
    len = sdslen(host_msg_sever);
    if (sendall(client_socket, host_msg_sever, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }

    chirc_message_destroy(msg);

    /* RPL_ENDOFWHOIS */
    msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
    chirc_message_construct(msg, msg_prefix, "318");
    chirc_message_add_parameter(msg, s->info.nick, false);
    chirc_message_add_parameter(msg, cmdtokens[1], false);
    chirc_message_add_parameter(msg, "End of WHOIS list\r\n", true);
    sds host_msg_end;
    chirc_message_to_string(msg, &host_msg_end);
    chilog(INFO, "%s\n", host_msg_end);

    len = sdslen(host_msg_end);
    if (sendall(client_socket, host_msg_end, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
    chirc_message_destroy(msg);
    return 1;
}

int handle_LIST(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    channel_t *channel = NULL;
    sds channel_name = sdsempty();
    int num_clients = 0;
    sds msg_prefix = sdsnew(":bar.example.com"); // reply msg prefix

    client_t **client_hashtable = &ctx->client_hashtable;
    pthread_mutex_lock(&ctx->clients_lock);
    client_t *s = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);
    if (s == NULL) // not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, client_socket, client_hashtable);
        return -1;
    }
    if (s->info.state != REGISTERED) // not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, client_socket, client_hashtable);
        return -1;
    }

    sds msg_all = sdsempty();

    if (argc == 1)
    {
        pthread_mutex_lock(&ctx->channels_lock);
        for (channel = ctx->channels_hashtable; channel != NULL; channel = channel->hh.next)
        {
            channel_name = channel->channel_name;
            num_clients = HASH_COUNT(channel->channel_clients);
            chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
            chirc_message_construct(msg, msg_prefix, RPL_LIST);
            chirc_message_add_parameter(msg, s->info.nick, false);
            chirc_message_add_parameter(msg, channel_name, false);
            chirc_message_add_parameter(msg, sdsfromlonglong(num_clients), false);
            chirc_message_add_parameter(msg, "\r\n", true);
            sds host_msg;
            chirc_message_to_string(msg, &host_msg);
            chilog(INFO, "list: %s\n", host_msg);
            msg_all = sdscatsds(msg_all, host_msg);
            chirc_message_destroy(msg);
            sdsfree(host_msg);
        }
        pthread_mutex_unlock(&ctx->channels_lock);
    }
    else if (argc == 2)
    {
        channel_t **channel_table = &ctx->channels_hashtable;
        pthread_mutex_lock(&ctx->channels_lock);
        channel_name = find_CHANNEL(cmdtokens[1], channel_table)->channel_name;
        num_clients = HASH_COUNT(ctx->channels_hashtable->channel_clients);
        pthread_mutex_unlock(&ctx->channels_lock);

        chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
        chirc_message_construct(msg, msg_prefix, RPL_LIST);
        chirc_message_add_parameter(msg, s->info.nick, false);
        chirc_message_add_parameter(msg, channel_name, false);
        chirc_message_add_parameter(msg, sdsfromlonglong(num_clients), false);
        chirc_message_add_parameter(msg, "\r\n", false);
        sds host_msg;
        chirc_message_to_string(msg, &host_msg);
        chilog(INFO, "list: %s\n", host_msg);
        msg_all = sdscatsds(msg_all, host_msg);
        chirc_message_destroy(msg);
    }
    
    int len = sdslen(msg_all);
    if (sendall(client_socket, msg_all, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
    sdsfree(msg_all);
    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
    chirc_message_construct(msg, msg_prefix, RPL_LISTEND);
    chirc_message_add_parameter(msg, s->info.nick, false);
    chirc_message_add_parameter(msg, "End of LIST\r\n", true);
    sds host_msg;
    chirc_message_to_string(msg, &host_msg);
    chilog(INFO, "%s\n", host_msg);
    chirc_message_destroy(msg);
    
    len = sdslen(host_msg);
    if (sendall(client_socket, host_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
    return 1;
}

int handle_MODE(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    if (argc - 1 < 3)
    {
        return 0;
    }
    char *channel_name = cmdtokens[1];
    char *mode = cmdtokens[2];
    char *nick = cmdtokens[3];

    client_t **client_hashtable = &ctx->client_hashtable;
    pthread_mutex_lock(&ctx->clients_lock);
    client_t *client = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);

    channel_t **channel_hashtable = &ctx->channels_hashtable;
    pthread_mutex_lock(&ctx->channels_lock);
    channel_t *channel = find_CHANNEL(channel_name, channel_hashtable);
    pthread_mutex_unlock(&ctx->channels_lock);

    if (channel == NULL)
    {
        /* ERR_NOSUCHCHANNEL */
        reply_error(cmdtokens, ERR_NOSUCHCHANNEL, client_socket, &client);
        return 0;
    }
    if (!strcmp(mode, "+o") && !strcmp(mode, "-o"))
    {
        /* UNKNOWNMODE */
        reply_error(cmdtokens, ERR_UNKNOWNMODE, client_socket, &client);
        return 0;
    }
    pthread_mutex_lock(&ctx->channels_lock);
    channel_client *chan = find_CHANNEL_CLIENT(nick, &channel->channel_clients);
    pthread_mutex_unlock(&ctx->channels_lock);
    if (chan == NULL)
    {
        /* ERR_USERNOTINCHANNEL */
        reply_error(cmdtokens, ERR_USERNOTINCHANNEL, client_socket, &client);
        return 0;
    }
    if (!strcmp(chan->mode, "+o") && !(client->info.is_irc_operator))
    {
        /* ERR_CHANOPRIVSNEEDED */
        reply_error(cmdtokens, ERR_USERNOTINCHANNEL, client_socket, &client);
        return 0;
    }

    chan->mode = mode;
    pthread_mutex_lock(&ctx->channels_lock);
    /* send msg to each client in the channel */
    for (chan = channel->channel_clients; chan != NULL; chan = channel->hh.next)
    {

        /* do not send msg to self */
        if (strcmp(chan->nick, client->info.nick) == 0)
        {
            continue;
        }
        pthread_mutex_lock(&ctx->nicks_lock);
        nick_t *msgtarget = find_NICK(chan->nick, &ctx->nicks_hashtable);
        pthread_mutex_unlock(&ctx->nicks_lock);
        chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
        sds prefix = sdscatprintf(sdsempty(), ":%s!%s@foo.example.com", client->info.nick, client->info.nick);
        chirc_message_construct(msg, prefix, "MODE");
        chirc_message_add_parameter(msg, channel_name, false);
        sds param = sdscatprintf(param, "%s\r\n", mode);
        chirc_message_add_parameter(msg, param, true);
        sds host_msg;
        chirc_message_to_string(msg, &host_msg);

        send(msgtarget->client_socket, host_msg, sdslen(host_msg), 0);
        sdsfree(prefix);
        sdsfree(host_msg);
        sdsfree(param);
        chirc_message_destroy(msg);
    }
    pthread_mutex_unlock(&ctx->channels_lock);
    // chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
    // chirc_message_construct(msg, channel_name, "MODE");
    // chirc_message_add_parameter(msg, nick, false);
    // sds msgs = sdscatprintf(msgs, "%s\r\n", mode);
    // chirc_message_add_parameter(msg, msgs, true);
    // sds host_msg;
    // chirc_message_to_string(msg, &host_msg);
    // chirc_message_destroy(msg);

    // /*relay to all nicks in the channel, for all nicks, find socket*/

    // channel_client *channel_client = NULL;
    // for (channel_client = channel->channel_clients; channel_client != NULL; channel_client = channel_client->hh.next)
    // {
    //     pthread_mutex_lock(&ctx->nicks_lock);
    //     int socket = find_NICK(channel_client->nick, &ctx->nicks_hashtable)->client_socket;
    //     send(socket, host_msg, sdslen(host_msg), 0);
    //     pthread_mutex_unlock(&ctx->nicks_lock);
    // }

    // sdsfree(host_msg);

    return 1;
}
int handle_OPER(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    client_t **client_hashtable = &ctx->client_hashtable;
    pthread_mutex_lock(&ctx->clients_lock);
    client_t *client = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);

    if (argc - 1 < OPER_PARAMETER_NUM)
    {
        // ERR_NEEDMOREPARAMS
        reply_error(cmdtokens, ERR_NEEDMOREPARAMS, client_socket, client_hashtable);
        return 0;
    }
    if (!strcmp(cmdtokens[2], ctx->password))
    {
        // ERR_PASSWDMISMATCH
        reply_error(cmdtokens, ERR_PASSWDMISMATCH, client_socket, client_hashtable);
        return 0;
    }

    // add client to hash table of irc_operator
    irc_oper_t *irc_operator_value;
    pthread_mutex_lock(&ctx->operators_lock);
    HASH_FIND_STR(ctx->irc_operators_hashtable, cmdtokens[2], irc_operator_value);
    pthread_mutex_unlock(&ctx->operators_lock);

    if (irc_operator_value != NULL)
    {
        // RPL_YOUREOPER
        chilog(ERROR, "IRC Operator already exists");
        return 0;
    }

    irc_oper_t *irc_oper_add = (irc_oper_t *)malloc(sizeof(irc_oper_t));
    irc_oper_add->nick = sdsempty();
    irc_oper_add->nick = sdscpy(irc_oper_add->nick, cmdtokens[2]);
    irc_oper_add->mode = "+o";
    pthread_mutex_lock(&ctx->operators_lock);
    HASH_ADD_STR(ctx->irc_operators_hashtable, nick, irc_oper_add);
    pthread_mutex_unlock(&ctx->operators_lock);

    client->info.is_irc_operator = true;

    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
    chirc_message_construct(msg, server_hostname, RPL_YOUREOPER);
    chirc_message_add_parameter(msg, cmdtokens[2], false);
    chirc_message_add_parameter(msg, "You are now an IRC operator\r\n", true);
    sds host_msg;
    chirc_message_to_string(msg, &host_msg);
    chirc_message_destroy(msg);
    send(client_socket, host_msg, sdslen(host_msg), 0);

    return 0;
}

int handle_PART(server_ctx *ctx, int client_socket, sds *cmdtokens, int argc, char *server_hostname)
{
    int len;
    client_t **client_hashtable = &ctx->client_hashtable;
    pthread_mutex_lock(&ctx->clients_lock);
    client_t *s = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);
    char *prefix = ":bar.example.com";
    if (s == NULL) // not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, client_socket, client_hashtable);
        return -1;
    }
    if (s->info.state != REGISTERED) // not registered
    {
        /* ERR_NOTREGISTERED */
        chilog(ERROR, "ERR_NOTREGISTERED\n");
        reply_error(cmdtokens, ERR_NOTREGISTERED, client_socket, client_hashtable);
        return -1;
    }

    /* ERR_NEEDMOREPARAMS */
    if (argc - 1 < PART_PARAMETER_NUM)
    {
        reply_error(cmdtokens, ERR_NEEDMOREPARAMS, client_socket, client_hashtable);
        return 0;
    }

    channel_t **channel_hashtable = &ctx->channels_hashtable;
    pthread_mutex_lock(&ctx->channels_lock);
    channel_t *c = find_CHANNEL(cmdtokens[1], channel_hashtable);
    pthread_mutex_unlock(&ctx->channels_lock);
    if (c == NULL) // channel not exist
    {
        chilog(ERROR, "ERR_NOSUCHCHANNEL\n");
        reply_error(cmdtokens, ERR_NOSUCHCHANNEL, client_socket, client_hashtable);
        return -1;
    }
    channel_client *cc = NULL;
    pthread_mutex_lock(&ctx->channels_lock);
    cc = find_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
    pthread_mutex_unlock(&ctx->channels_lock);
    if (cc == NULL) // client not in the channel
    {
        chilog(ERROR, "ERR_NOTONCHANNEL\n");
        reply_error(cmdtokens, ERR_NOTONCHANNEL, client_socket, client_hashtable);
        return -1;
    }

    /* leave the channel */

    /* send msg to each client in the channel */
    for (cc = c->channel_clients; cc != NULL; cc = cc->hh.next)
    {
        pthread_mutex_lock(&ctx->nicks_lock);
        nick_t *msgtarget = find_NICK(cc->nick, &ctx->nicks_hashtable);
        pthread_mutex_unlock(&ctx->nicks_lock);
        chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));
        sds prefix = sdscatprintf(sdsempty(), ":%s!%s@foo.example.com", s->info.nick, s->info.nick);
        chirc_message_construct(msg, prefix, "PART");

        if (argc > 2) /* if has part msg */
        {
            chirc_message_add_parameter(msg, c->channel_name, false);
            sds part_msg = sdsempty();
            part_msg = sdsjoinsds(cmdtokens + 2, argc - 2, " ", 1);
            if (part_msg[0] == ':')
                sdsrange(part_msg, 1, sdslen(part_msg));
            chilog(INFO, "PART with: %s\n", part_msg);
            part_msg = sdscat(part_msg, "\r\n");
            chirc_message_add_parameter(msg, part_msg, true);
        }
        else
        {
            sds param = sdscatprintf(sdsempty(), "%s\r\n", c->channel_name);
            chirc_message_add_parameter(msg, param, false);
        }
        sds host_msg;
        chirc_message_to_string(msg, &host_msg);

        len = sdslen(host_msg);
        if (sendall(msgtarget->client_socket, host_msg, &len) == -1) {
            perror("sendall");
            printf("We only sent %d bytes because of the error!\n", len);
        }

        sdsfree(prefix);
        sdsfree(host_msg);
        chirc_message_destroy(msg);
    }
    remove_CHANNEL_CLIENT(s->info.nick, &c->channel_clients);
    if (HASH_COUNT(c->channel_clients) <= 0)
    {
        remove_CHANNEL(c->channel_name, channel_hashtable);
    }
    return 1;
}

void reply_registration(server_ctx *ctx, sds *cmd, int argc, int client_socket, client_t ** client_hashtable, char *server_hostname)
{
    sds nick = sdsempty();
    client_t *s = find_USER(client_socket, client_hashtable);

    if (s == NULL)
    {
        nick = sdscpy(nick, "*");
    }
    else if (s->info.state == NICK_MISSING)
    {
        nick = sdscpy(nick, "*");
    }
    else
    {
        nick = sdscpy(nick, s->info.nick);
    }
    char *prefix = ":bar.example.com"; // reply msg prefix
    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    /* RPL_WELCOME */
    chirc_message_construct(msg, prefix, RPL_WELCOME);
    chirc_message_add_parameter(msg, nick, false);
    sds param = sdscatprintf(sdsempty(), "Welcome to the Internet Relay Network %s!%s@%s\r\n", s->info.nick, s->info.username, CLIENTHOST);
    chirc_message_add_parameter(msg, param, true);
    sds host_msg;
    chirc_message_to_string(msg, &host_msg);

    int len = sdslen(host_msg);
    if (sendall(client_socket, host_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
    chirc_message_destroy(msg);

    /* RPL_YOURHOST */
    chirc_message_construct(msg, prefix, RPL_YOURHOST);
    chirc_message_add_parameter(msg, nick, false);
    param = sdscatprintf(sdsempty(), "Your host is %s, running version %s\r\n", SEVERHOST, VERSION);
    chirc_message_add_parameter(msg, param, true);
    chirc_message_to_string(msg, &host_msg);

    len = sdslen(host_msg);
    if (sendall(client_socket, host_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
    chirc_message_destroy(msg);

    /* RPL_CREATED */
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    chirc_message_construct(msg, prefix, RPL_CREATED);
    chirc_message_add_parameter(msg, nick, false);
    param = sdscatprintf(sdsempty(), "This server was created %d-%02d-%02d %02d:%02d:%02d\r\n", tm.tm_year + 1900, tm.tm_mon + 1, 
                                    tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    chirc_message_add_parameter(msg, param, true);
    chirc_message_to_string(msg, &host_msg);

    len = sdslen(host_msg);
    if (sendall(client_socket, host_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
    chirc_message_destroy(msg);

    /*RPL_MYINFO */
    chirc_message_construct(msg, prefix, RPL_MYINFO);
    chirc_message_add_parameter(msg, nick, false);
    param = sdscatprintf(sdsempty(), "%s %s %s %s\r\n", SEVERHOST, VERSION, "ao", "mtov");
    chirc_message_add_parameter(msg, param, true);
    chirc_message_to_string(msg, &host_msg);

    len = sdslen(host_msg);
    if (sendall(client_socket, host_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
    chirc_message_destroy(msg);

    handle_LUSERS(ctx, client_socket, cmd, argc, server_hostname);

    /* ERR_NOMOTD */
    chirc_message_construct(msg, prefix, ERR_NOMOTD);
    chirc_message_add_parameter(msg, nick, false);
    param = sdscatprintf(sdsempty(), "MOTD File is missing\r\n");
    chirc_message_add_parameter(msg, param, true);
    chirc_message_to_string(msg, &host_msg);

    len = sdslen(host_msg);
    if (sendall(client_socket, host_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
    chirc_message_destroy(msg);
    sdsfree(host_msg);
    sdsfree(param);
}