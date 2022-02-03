#include <sys/socket.h>
#include "reply.h"
#include "client.h"
#include "msg.h"
#include "../lib/sds/sds.h"
#include "sendall.h"
void reply_error(sds *cmd, char *reply_code, int client_socket, client_t **client_hashtable)
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
    chirc_message_construct(msg, prefix, reply_code);
    chirc_message_add_parameter(msg, nick, false);

    if (strcmp(reply_code, ERR_UNKNOWNCOMMAND) == 0)
    {
        chirc_message_add_parameter(msg, cmd[0], false);
        sds error = sdscatprintf(sdsempty(), "Unknown command\r\n");
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_NONICKNAMEGIVEN) == 0)
    {
        sds error = sdscatprintf(sdsempty(), "No nickname given\r\n");
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_NICKNAMEINUSE) == 0)
    {
        chirc_message_add_parameter(msg, cmd[1], false);
        sds error = sdscatprintf(sdsempty(), "Nickname is already in use\r\n");
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_NEEDMOREPARAMS) == 0)
    {
        chirc_message_add_parameter(msg, cmd[0], false);
        sds error = sdscatprintf(sdsempty(), "Not enough parameters\r\n");
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_ALREADYREGISTRED) == 0)
    {
        sds error = sdscatprintf(sdsempty(), "Unauthorized command (already registered)\r\n");
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_NOSUCHNICK) == 0)
    {
        chirc_message_add_parameter(msg, cmd[1], false);
        sds error = sdscatprintf(sdsempty(), "No such nick/channel\r\n");
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_NOMOTD) == 0)
    {
        sds error = sdscatprintf(sdsempty(), "MOTD File is missing\r\n");
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_NOTREGISTERED) == 0)
    {
        sds error = sdscatprintf(sdsempty(), "You have not registered\r\n");
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_NORECIPIENT) == 0)
    {
        sds error = sdscatprintf(sdsempty(), "No recipient given (%s)\r\n", cmd[0]);
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_NOTEXTTOSEND) == 0)
    {
        sds error = sdscatprintf(sdsempty(), "No text to send\r\n");
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_NOSUCHCHANNEL) == 0)
    {
        chirc_message_add_parameter(msg, cmd[1], false);
        sds error = sdscatprintf(sdsempty(), "No such channel\r\n");
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_UNKNOWNMODE) == 0)
    {
        chirc_message_add_parameter(msg, cmd[1], false);
        sds error = sdscatprintf(sdsempty(), "%s is unknown mode char to me for %s\r\n", cmd[2], cmd[1]);
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_CHANOPRIVSNEEDED) == 0)
    {
        sds error = sdscatprintf(sdsempty(), "%s %s You're not channel operator\r\n", cmd[0], cmd[1]);
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_USERNOTINCHANNEL) == 0)
    {
        sds error = sdscatprintf(sdsempty(), "%s %s They aren't on that channel\r\n", cmd[3], cmd[1]);
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_CANNOTSENDTOCHAN) == 0)
    {
        chirc_message_add_parameter(msg, cmd[1], false);
        sds error = sdscatprintf(sdsempty(), "Cannot send to channel\r\n");
        chirc_message_add_parameter(msg, error, true);
    }
    else if (strcmp(reply_code, ERR_NOTONCHANNEL) == 0)
    {
        chirc_message_add_parameter(msg, cmd[1], false);
        sds error = sdscatprintf(sdsempty(), "You're not on that channel\r\n");
        chirc_message_add_parameter(msg, error, true);
    }
    else
    {
        return;
    }

    sds host_msg;
    chirc_message_to_string(msg, &host_msg);

    int len = sdslen(host_msg);
    if (sendall(client_socket, host_msg, &len) == -1) {
        perror("sendall");
        printf("We only sent %d bytes because of the error!\n", len);
    }
}




    