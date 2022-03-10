#include "server_cmd.h"


client_t *server_find_USER(server_ctx *ctx, int client_socket)
{
    /*
     * server_find_USER - (Thread-safe) Find connected client from client_hashtable table if exists.
     *
     * ctx: server_context
     *
     * client_socket: client socket for the reply
     *
     * Return: The searched client_t pointer.
     *
     */

    client_t **client_hashtable = &ctx->client_hashtable;

    pthread_mutex_lock(&ctx->clients_lock);
    client_t *user = find_USER(client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);

    return user;
}


nick_t *server_find_NICK(server_ctx *ctx, sds nickname)
{
    /*
     * server_find_NICK - (Thread-safe)Find nickname from nickname table if exists
     *
     * ctx: server_context
     *
     * nickname: The nickname you want to search as key
     *
     * Return: The nick with given nickname or NULL if not exists.
     *
     */
    nick_t **nicks_hashtable = &ctx->nicks_hashtable;

    pthread_mutex_lock(&ctx->nicks_lock);
    nick_t *nick = find_NICK(nickname, nicks_hashtable);
    pthread_mutex_unlock(&ctx->nicks_lock);

    return nick;
}


irc_oper_t *server_find_OPER(server_ctx *ctx, sds nickname)
{
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
    irc_oper_t *irc_operator_value;

    pthread_mutex_lock(&ctx->operators_lock);
    HASH_FIND_STR(ctx->irc_operators_hashtable, nickname, irc_operator_value);
    pthread_mutex_unlock(&ctx->operators_lock);

    return irc_operator_value;
}


channel_client *server_find_CHANNEL_CLIENT(server_ctx *ctx, channel_t *channel, sds nickname)
{
    /*
     * server_find_CHANNEL_CLIENT - (Thread-safe)Find the client in channel with the given nick name
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

    pthread_mutex_lock(&ctx->channels_lock);
    channel_client *cha_cli = find_CHANNEL_CLIENT(nickname, &channel->channel_clients);
    pthread_mutex_unlock(&ctx->channels_lock);

    return cha_cli;
}


channel_t *server_find_CHANNEL(server_ctx *ctx, sds channel_name)
{
    /*
     * server_find_CHANNEL - (Thread-safe) Find channel with the given channel name
     *
     * ctx: server_context
     *
     * client_socket: client socket for the reply
     *
     * Return: The channel with given name or NULL if not exists.
     *
     */
    channel_t **channel_hashtable = &ctx->channels_hashtable;

    pthread_mutex_lock(&ctx->channels_lock);
    channel_t *channel = find_CHANNEL(channel_name, channel_hashtable);
    pthread_mutex_unlock(&ctx->channels_lock);

    return channel;
}


nick_t *server_add_NICK(server_ctx *ctx, int client_socket, sds nickname)
{
    /*
     * server_add_NICK - (Thread-safe)Add nickname with the socket to the given nickname from nicks hashtable if not
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
    nick_t **nicks_hashtable = &ctx->nicks_hashtable;

    pthread_mutex_lock(&ctx->nicks_lock);
    nick_t *added = add_NICK(nickname, client_socket, nicks_hashtable);
    pthread_mutex_unlock(&ctx->nicks_lock);

    return added;
}


client_t *server_add_USER(server_ctx *ctx, client_t *client, int client_socket)
{
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
    client_t **client_hashtable = &ctx->client_hashtable;

    pthread_mutex_lock(&ctx->clients_lock);
    client_t *added = add_USER(client, client_socket, client_hashtable);
    pthread_mutex_unlock(&ctx->clients_lock);

    return added;
}


void server_remove_NICK(server_ctx *ctx, sds nickname)
{
    /*
     * server_remove_NICK - (Thread-safe)Find channel with the given channel name
     *
     * ctx: server_context
     *
     * nickname: nickname to be removed
     *
     * Return: nothing
     */
    nick_t **nicks_hashtable = &ctx->nicks_hashtable;

    pthread_mutex_lock(&ctx->nicks_lock);
    remove_NICK(nickname, nicks_hashtable);
    pthread_mutex_unlock(&ctx->nicks_lock);
}


channel_t *server_add_CHANNEL(server_ctx *ctx, sds channel_name)
{
    /*
     * server_add_CHANNEL - (Thread-safe)Add channel with the given channel name
     *
     * ctx: server_context
     *
     * channel_name: channel_name to be added
     *
     * Return: The channel added.
     *
     */
    channel_t **channel_hashtable = &ctx->channels_hashtable;

    pthread_mutex_lock(&ctx->channels_lock);
    channel_t *channel = add_CHANNEL(channel_name, channel_hashtable);
    pthread_mutex_unlock(&ctx->channels_lock);

    return channel;
}


channel_client *server_add_CHANNEL_CLIENT(server_ctx *ctx, sds nickname, sds channel_name, bool flag)
{
    /*
     * server_add_CHANNEL_CLIENT - (Thread-safe) Add client with the given channel name to the channel
     *
     * ctx: server_context
     *
     * nickname: The nickname you want to insert the client into the channel as key
     *
     * channel_name: channel_name to be added
     *
     * flag: If flag == 1, channel exists, else channel is newly created.
     *
     * Return: The channel_client with given nickname after adding it to the hashtable.
     *
     */
    channel_t *c = server_find_CHANNEL(ctx, channel_name);

    pthread_mutex_lock(&ctx->channels_lock);
    channel_client *cha_cli = add_CHANNEL_CLIENT(nickname, &c->channel_clients);
    if (flag == 0)
    {
        cha_cli->mode = "o";
    }
    else
    {
        cha_cli->mode = "-o";
    }
    pthread_mutex_unlock(&ctx->channels_lock);

    return cha_cli;
}


irc_oper_t *server_add_OPER(server_ctx *ctx, irc_oper_t *irc_operator_value)
{
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
    pthread_mutex_lock(&ctx->operators_lock);
    HASH_ADD_STR(ctx->irc_operators_hashtable, nick, irc_operator_value);
    pthread_mutex_unlock(&ctx->operators_lock);

    return irc_operator_value;
}


void add_connected_user_number(server_ctx *ctx)
{
    /*
     * add_connected_user_number - (Thread-safe)add connected user number
     *
     * ctx: server_context
     *
     * Return: nothing
     */
    pthread_mutex_lock(&ctx->lock);
    ctx->num_connected_users++;
    pthread_mutex_unlock(&ctx->lock);
}


void dec_connected_user_number(server_ctx *ctx)
{
    /*
     * dec_connected_user_number - (Thread-safe)decrease connected user number
     *
     * ctx: server_context
     *
     * Return: nothing
     */
    pthread_mutex_lock(&ctx->lock);
    ctx->num_connected_users--;
    pthread_mutex_unlock(&ctx->lock);
}


void add_total_connected_number(server_ctx *ctx)
{
    /*
     * add_total_connected_number - (Thread-safe)add total connections number
     *
     * ctx: server_context
     *
     * Return: nothing
     */
    pthread_mutex_lock(&ctx->lock);
    ctx->total_connections++;
    pthread_mutex_unlock(&ctx->lock);
}


void dec_total_connected_number(server_ctx *ctx)
{
    /*
     * dec_total_connected_number - (Thread-safe)decrease total connections number
     *
     * ctx: server_context
     *
     * Return: nothing
     */
    pthread_mutex_lock(&ctx->lock);
    ctx->total_connections--;
    pthread_mutex_unlock(&ctx->lock);
}
