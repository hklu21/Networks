#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <string.h>

#include "msg.h"

#include "reply.h"
#define MAX_NPARAM 15
int chirc_message_from_string(chirc_message_t *msg, sds s)
{
    if (msg && s) {
        int i;
        sds *rpl_seg;
        int count;
        rpl_seg = sdssplitargs(s, &count); 
        if (count < 2) {    // need a prefix and cmd at least
            return -1;
        }
        msg->nparams = count - 2;
        
        msg->prefix = sdsnew(rpl_seg[0]);
        msg->cmd = sdsnew(rpl_seg[1]);
        msg->params = (sds *)malloc(msg->nparams * sizeof(sds));
        for (i = 2; i < count; i++){
            msg->params[i - 2] = sdsnew(rpl_seg[i]);
        }
        msg->longlast = false;  // the whole content of the last param is included
        sdsfreesplitres(rpl_seg, count);
        return 0;
    } else {
        return -1;
    }
}
int chirc_message_to_string(chirc_message_t *msg, sds *s){
    if (msg) {
        int i;
        *s = sdsempty();
        *s = sdscatsds(*s, msg->prefix);
        *s = sdscat(*s, " ");
        *s = sdscatsds(*s, msg->cmd);
        *s = sdscat(*s, " ");
        for (i = 0; i < msg->nparams - 1; i++) {
            *s = sdscatsds(*s, msg->params[i]);
            *s = sdscat(*s, " ");
        }
        /* prepend a conlon to s before the last param if the longlast is true*/
        if (msg->longlast) {
            *s = sdscat(*s, ":");
        }
        *s = sdscatsds(*s, msg->params[i]);
        return 0;
    } else {
        return -1;
    }
}

int chirc_message_construct(chirc_message_t *msg, char *prefix, char *cmd) {
    if (msg && prefix && cmd) {
        msg->prefix = sdsnew(prefix);
        msg->cmd = sdsnew(cmd);
        msg->params = (sds *) malloc(MAX_NPARAM * sizeof(sds));
        msg->nparams = 0;
        msg->longlast = false;
        return 0;
    } else {
        return -1;
    }
}

int chirc_message_add_parameter(chirc_message_t *msg, char *param, bool longlast){
    if (msg && param) {
        msg->longlast = longlast;
        msg->params[msg->nparams] = sdsnew(param);
        msg->nparams++;
        return 0;
    } else {
        return -1;
    }
}

int chirc_message_destroy(chirc_message_t *msg){
    if (msg){
        sdsfree(msg->prefix);
        sdsfree(msg->cmd);
        sdsfreesplitres(msg->params, msg->nparams);
        free(msg);
        return 0;
    } else {
        return -1;
    }
}