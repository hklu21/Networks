#ifndef SENDALL_H
#define SENDALL_H
#include <sys/types.h>
#include <sys/socket.h>

/* safe method to send all msg to client */
int sendall(int s, char *buf, int *len);

#endif
