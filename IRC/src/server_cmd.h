#include "server.h"
#include "client.h"
#include "../lib/sds/sds.h"
#include "channels.h"

/*
 * server_find_USER - (Thread-safe) Find connected client
 * from client_hashtable table if exists.
 *
 * ctx: server_context
 *
 * client_socket: client socket for the reply
 *
 * Return: The searched client_t pointer.
 *
 */
client_t *server_find_USER(server_ctx *ctx, int client_socket);

/*
 * server_find_CHANNEL - (Thread-safe) Find channel with the given
 * channel name
 *
 * ctx: server_context
 *
 * client_socket: client socket for the reply
 *
 * Return: The channel with given name or NULL if not exists.
 *
 */
channel_t *server_find_CHANNEL(server_ctx *ctx, sds channel_name);

/*
 * server_find_NICK - (Thread-safe)Find nickname from nickname table
 * if exists
 *
 * ctx: server_context
 *
 * nickname: The nickname you want to search as key
 *
 * Return: The nick with given nickname or NULL if not exists.
 *
 */
nick_t *server_find_NICK(server_ctx *ctx, sds nickname);

/*
 * server_find_CHANNEL_CLIENT - (Thread-safe)Find the client in
 * channel with the given nick name
 *
 * ctx: server_context
 *
 * channel: the channel which may include the client
 *
 * nickname: the nickname to be searched as key
 *
 * Return: The channel_client with given nickname or NULL if not exists.
 *
 */
channel_client *server_find_CHANNEL_CLIENT(server_ctx *ctx,
                                           channel_t *channel, sds nickname);

/*
 * server_find_OPER - (Thread-safe)Find operator with the given nickname
 *
 * ctx: server_context
 *
 *  nickname: the nickname to be searched as key
 *
 * Return: The operator pointer with given name or NULL if not exists.
 *
 */
irc_oper_t *server_find_OPER(server_ctx *ctx, sds nickname);

/*
 * server_add_NICK - (Thread-safe)Add nickname with the socket to
 * the given nickname from nicks hashtable if not
 * exists; If exists, it will return the searched value
 *
 * ctx: server_context
 *
 * client_socket: client socket for the reply
 *
 * nickname: the nickname to be inserted as key
 *
 * Return: The nick added.
 *
 */
nick_t *server_add_NICK(server_ctx *ctx, int client_socket, sds nickname);

/*
 * server_add_USER - (Thread-safe)Add client to client_hashtable table
 *
 * ctx: server_context
 *
 * client: the user to be added
 *
 * client_socket: client socket for the reply
 *
 * Return: The channel with given name or NULL if not exists.
 *
 */
client_t *server_add_USER(server_ctx *ctx, client_t *client,
                          int client_socket);

/*
 * server_add_CHANNEL - (Thread-safe)Add channel with the given
 * channel name
 *
 * ctx: server_context
 *
 * channel_name: channel_name to be added
 *
 * Return: The channel added.
 *
 */
channel_t *server_add_CHANNEL(server_ctx *ctx, sds channel_name);

/*
 * server_add_CHANNEL_CLIENT - (Thread-safe) Add client with the
 * given channel name to the channel
 *
 * ctx: server_context
 *
 * nickname: The nickname you want to insert the client into the
 * channel as key
 *
 * channel_name: channel_name to be added
 *
 * flag: If flag == 1, channel exists, else channel is newly created.
 *
 * Return: The channel_client with given nickname after adding it to
 * the hashtable.
 *
 */
channel_client *server_add_CHANNEL_CLIENT(server_ctx *ctx,
                                          sds nickname, sds channel_name, 
                                          bool flag);

/*
 * server_add_OPER - (Thread-safe)Add operator to the hashtable
 *
 * ctx: server_context
 *
 * irc_operator_value: operator to be added
 *
 * Return: The added operator.
 *
 */
irc_oper_t *server_add_OPER(server_ctx *ctx, irc_oper_t *irc_operator_value);

/*
 * server_remove_NICK - (Thread-safe)Find channel with the given
 * channel name
 *
 * ctx: server_context
 *
 * nickname: nickname to be removed
 *
 * Returns: nothing
 */
void server_remove_NICK(server_ctx *ctx, sds nickname);

/*
 * add_connected_user_number - (Thread-safe)add connected user number
 *
 * ctx: server_context
 *
 * Returns: nothing
 */
void add_connected_user_number(server_ctx *ctx);

/*
 * dec_connected_user_number - (Thread-safe)decrease connected user number
 *
 * ctx: server_context
 *
 * Returns: nothing
 */
void dec_connected_user_number(server_ctx *ctx);

/*
 * add_total_connected_number - (Thread-safe)add total connections number
 *
 * ctx: server_context
 *
 * Returns: nothing
 */
void add_total_connected_number(server_ctx *ctx);

/*
 * dec_total_connected_number - (Thread-safe)decrease total
 * connections number
 *
 * ctx: server_context
 *
 * Returns: nothing
 */
void dec_total_connected_number(server_ctx *ctx);