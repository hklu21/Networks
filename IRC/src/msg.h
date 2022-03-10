#ifndef MSG_H
#define MSG_H

#include "../lib/uthash.h"
#include "../lib/sds/sds.h"

typedef struct
{
    sds prefix;           // message prefix
    sds cmd;              // message cmd code, could be reply_code or command or error_code
    sds *params;          // parameters to be added to message
    unsigned int nparams; // number of parameters added to the message
    bool longlast;        // If true, a ":" saperator will be added to the message; else no saperator will be added
} chirc_message_t;

/*
 * chirc_message_from_string -  Transform string to messages
 *
 * msg: The chirc_message_t to hold the string
 *
 * s: The sds string to be transformed
 *
 * Return: MSG_OK/CHIRC_ERROR
 *
 * The function is not used in the assignment
 */
int chirc_message_from_string(chirc_message_t *msg, sds s);

/*
 * chirc_message_to_string - Transform messages to string
 *
 * msg: The chirc_message_t holding the message to be converted
 *
 * s: The sds string to hold the transformed message
 *
 * Return: MSG_OK/CHIRC_ERROR
 *
 * The function is used before sending the message.
 */
int chirc_message_to_string(chirc_message_t *msg, sds *s);

/*
 * chirc_message_construct - Construct the message
 *
 * msg: The chirc_message_t holding the message
 *
 * prefix: The prefix to be added to message
 *
 * cmd: The cmd to be added to after prefix message
 *
 * Return: MSG_OK/CHIRC_ERROR
 *
 * The function is used before creating the message.
 */
int chirc_message_construct(chirc_message_t *msg, char *prefix, char *cmd);

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
 * Return: MSG_OK/CHIRC_ERROR
 *
 * The function is used when adding parameter to the message.
 */
int chirc_message_add_parameter(chirc_message_t *msg, char *param, bool longlast);

/*
 * chirc_message_destroy - Destroy the message
 *
 * msg: The chirc_message_t message to be destroyed/freed
 *
 * Return: MSG_OK/CHIRC_ERROR
 *
 * The function is used after sending the message.
 */
int chirc_message_destroy(chirc_message_t *msg);

#endif