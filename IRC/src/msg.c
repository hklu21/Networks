#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "msg.h"
#include "reply.h"
#define MAX_NPARAM 15


int chirc_message_from_string(chirc_message_t *msg, sds s)
{
    /*
     * chirc_message_from_string -  Transform string to messages
     *
     * msg: The chirc_message_t to hold the string
     *
     * s: The sds string to be transformed
     *
     * Return: MSG_OK/MSG_ERROR
     *
     * The function is not used in the assignment
     */
    if (msg && s)
    {
        int i;
        sds *rpl_seg;
        int count;
        rpl_seg = sdssplitargs(s, &count);

        if (count < 2)
        { // need a prefix and cmd at least
            return MSG_ERROR;
        }

        msg->nparams = count - 2;
        msg->prefix = sdsnew(rpl_seg[0]);
        msg->cmd = sdsnew(rpl_seg[1]);
        msg->params = (sds *)malloc(msg->nparams * sizeof(sds));

        for (i = 2; i < count; i++)
        {
            msg->params[i - 2] = sdsnew(rpl_seg[i]);
        }

        msg->longlast = false; // the whole content of the last param is included

        sdsfreesplitres(rpl_seg, count);

        return MSG_OK;
    }
    else
    {
        return MSG_ERROR;
    }
}


int chirc_message_to_string(chirc_message_t *msg, sds *s)
{
    /*
     * chirc_message_to_string - Transform messages to string
     *
     * msg: The chirc_message_t holding the message to be converted
     *
     * s: The sds string to hold the transformed message
     *
     * Return: MSG_OK/MSG_ERROR
     *
     * The function is used before sending the message.
     */
    if (msg)
    {
        int i;
        *s = sdsempty();
        *s = sdscatsds(*s, msg->prefix);
        *s = sdscat(*s, " ");
        *s = sdscatsds(*s, msg->cmd);
        *s = sdscat(*s, " ");

        for (i = 0; i < msg->nparams - 1; i++)
        {
            *s = sdscatsds(*s, msg->params[i]);
            *s = sdscat(*s, " ");
        }

        /* prepend a conlon to s before the last param if the longlast is true*/
        if (msg->longlast)
        {
            *s = sdscat(*s, ":");
        }

        *s = sdscatsds(*s, msg->params[i]);

        return MSG_OK;
    }
    else
    {
        return MSG_ERROR;
    }
}


int chirc_message_construct(chirc_message_t *msg, char *prefix, char *cmd)
{
    /*
     * chirc_message_construct - Construct the message
     *
     * msg: The chirc_message_t holding the message
     *
     * prefix: The prefix to be added to message
     *
     * cmd: The cmd to be added to after prefix message
     *
     * Return: MSG_OK/MSG_ERROR
     *
     * The function is used before creating the message.
     */
    if (msg && prefix && cmd)
    {
        msg->prefix = sdsnew(prefix);
        msg->cmd = sdsnew(cmd);
        msg->params = (sds *)malloc(MAX_NPARAM * sizeof(sds));
        msg->nparams = 0;
        msg->longlast = false;

        return MSG_OK;
    }
    else
    {
        return MSG_ERROR;
    }
}


int chirc_message_add_parameter(chirc_message_t *msg, char *param, bool longlast)
{
    /*
     * chirc_message_add_parameter - Add parameter to message
     *
     * msg: The chirc_message_t holding the message
     *
     * param: The parameter to be added to message
     *
     * longlast: If true, a ":" saperator will be added before the parameter
     * or no saperator if false.
     *
     * Return: MSG_OK/MSG_ERROR
     *
     * The function is used when adding parameter to the message.
     */
    if (msg && param)
    {
        msg->longlast = longlast;
        msg->params[msg->nparams] = sdsnew(param);
        msg->nparams++;

        return MSG_OK;
    }
    else
    {
        return MSG_ERROR;
    }
}


int chirc_message_destroy(chirc_message_t *msg)
{
    /*
     * chirc_message_destroy - Destroy the message
     *
     * msg: The chirc_message_t message to be destroyed/freed
     *
     * Return: MSG_OK/MSG_ERROR
     *
     * The function is used after sending the message.
     */
    if (msg)
    {
        sdsfree(msg->prefix);
        sdsfree(msg->cmd);
        sdsfreesplitres(msg->params, msg->nparams);
        free(msg);
        
        return MSG_OK;
    }
    else
    {
        return MSG_ERROR;
    }
}