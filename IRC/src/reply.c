#include <sys/socket.h>
#include "reply.h"
#include "client.h"
#include "msg.h"
#include "../lib/sds/sds.h"
#include "server_cmd.h"
#include "send_msg.h"
#include "log.h"


int reply_error(sds *cmd, char *reply_code, conn_info_t *conn, server_ctx *ctx)
{
    /*
     * reply_error - Message handler to process error reply
     *
     * cmd: cmdstacks to be added to the message
     *
     * reply_code: The reply code above to be added to message
     *
     * conn: Use the client_socket, server_hostname, client_hostname of it in the reply message.
     *
     * The function is used to reply errors.
     */
    int client_socket = conn->client_socket;
    sds server_hostname = conn->server_hostname;
    sds client_hostname = conn->client_hostname;
    sds nick = sdsempty();

    client_t *s = server_find_USER(ctx, client_socket);

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

    sds prefix = sdscatsds(sdsnew(":"), server_hostname);
    chirc_message_t *msg = (chirc_message_t *)malloc(sizeof(chirc_message_t));

    chirc_message_construct(msg, prefix, reply_code);

    chirc_message_add_parameter(msg, nick, false);

    if (!strncmp(reply_code, ERR_UNKNOWNCOMMAND, ERROR_CODE_LEN))
    {
        chirc_message_add_parameter(msg, cmd[0], false);
        // chilog(TRACE, "%s %d\n", reply_code, ERROR_CODE_LEN);
        sds error = sdscatprintf(sdsempty(), "Unknown command\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_NONICKNAMEGIVEN, ERROR_CODE_LEN))
    {
        sds error = sdscatprintf(sdsempty(), "No nickname given\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_NICKNAMEINUSE, ERROR_CODE_LEN))
    {
        chirc_message_add_parameter(msg, cmd[1], false);
        sds error = sdscatprintf(sdsempty(), "Nickname is already in use\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_NEEDMOREPARAMS, ERROR_CODE_LEN))
    {
        chirc_message_add_parameter(msg, cmd[0], false);
        sds error = sdscatprintf(sdsempty(), "Not enough parameters\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_ALREADYREGISTRED, ERROR_CODE_LEN))
    {
        sds error = sdscatprintf(sdsempty(), "Unauthorized command (already registered)\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_NOSUCHNICK, ERROR_CODE_LEN))
    {
        chirc_message_add_parameter(msg, cmd[1], false);
        sds error = sdscatprintf(sdsempty(), "No such nick/channel\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_NOMOTD, ERROR_CODE_LEN))
    {
        sds error = sdscatprintf(sdsempty(), "MOTD File is missing\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_NOTREGISTERED, ERROR_CODE_LEN))
    {
        sds error = sdscatprintf(sdsempty(), "You have not registered\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_NORECIPIENT, ERROR_CODE_LEN))
    {
        sds error = sdscatprintf(sdsempty(), "No recipient given (%s)\r\n", cmd[0]);
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_NOTEXTTOSEND, ERROR_CODE_LEN))
    {
        sds error = sdscatprintf(sdsempty(), "No text to send\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_NOSUCHCHANNEL, ERROR_CODE_LEN))
    {
        chirc_message_add_parameter(msg, cmd[1], false);
        sds error = sdscatprintf(sdsempty(), "No such channel\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_UNKNOWNMODE, ERROR_CODE_LEN))
    {
        chirc_message_add_parameter(msg, cmd[1], false);
        sds error = sdscatprintf(sdsempty(), "%s is unknown mode char to me for %s\r\n", cmd[2], cmd[1]);
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_CHANOPRIVSNEEDED, ERROR_CODE_LEN))
    {
        sds error = sdscatprintf(sdsempty(), "%s :You're not channel operator\r\n", cmd[1]);
        chirc_message_add_parameter(msg, error, false);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_USERNOTINCHANNEL, ERROR_CODE_LEN))
    {
        sds error = sdscatprintf(sdsempty(), "%s %s They aren't on that channel\r\n", cmd[3], cmd[1]);
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_CANNOTSENDTOCHAN, ERROR_CODE_LEN))
    {
        chirc_message_add_parameter(msg, cmd[1], false);
        sds error = sdscatprintf(sdsempty(), "Cannot send to channel\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_NOTONCHANNEL, ERROR_CODE_LEN))
    {
        chirc_message_add_parameter(msg, cmd[1], false);
        sds error = sdscatprintf(sdsempty(), "You're not on that channel\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else if (!strncmp(reply_code, ERR_PASSWDMISMATCH, ERROR_CODE_LEN))
    {
        sds error = sdscatprintf(sdsempty(), "Password incorrect\r\n");
        chirc_message_add_parameter(msg, error, true);

        sdsfree(error);
    }
    else
    {
        return MSG_ERROR;
    }
    
    sds host_msg;
    chirc_message_to_string(msg, &host_msg);
    int res = send_msg(client_socket, ctx, host_msg);

    sdsfree(host_msg);
    sdsfree(prefix);
    chirc_message_destroy(msg);

    return res;
}
