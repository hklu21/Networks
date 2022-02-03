#ifndef MSG_H
#define MSG_H

#include "../lib/uthash.h"
#include "../lib/sds/sds.h"

typedef struct {
    sds prefix;
    sds cmd;
    sds *params;
    unsigned int nparams;
    bool longlast;
} chirc_message_t;

int chirc_message_from_string(chirc_message_t *msg, sds s);
int chirc_message_to_string(chirc_message_t *msg, sds *s);

int chirc_message_construct(chirc_message_t *msg, char *prefix, char *cmd);
int chirc_message_add_parameter(chirc_message_t *msg, char *param, bool longlast);
int chirc_message_destroy(chirc_message_t *msg);
#endif