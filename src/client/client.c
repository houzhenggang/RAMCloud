/* Copyright (c) 2009 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>
#include <assert.h>
#include <malloc.h>

#include <config.h>

#include <shared/rcrpc.h>
#include <shared/net.h>

#include <client/client.h>

int
rc_connect(struct rc_client *client)
{
    rc_net_init(&client->net, CLNTADDR, CLNTPORT, SVRADDR, SVRPORT);
    rc_net_connect(&client->net);
    return 0;
}

void
rc_disconnect(struct rc_client *client)
{
}

// TODO(stutsman) we should put this in the client struct
enum { ERROR_MSG_LEN = 256 };
static char rc_error_message[ERROR_MSG_LEN];

const char*
rc_last_error()
{
    return &rc_error_message[0];
}

static int
rc_handle_errors(struct rcrpc *resp)
{
    if (resp->type != RCRPC_ERROR_RESPONSE)
        return 0;
    printf("... '%s'\n", resp->error_response.message);
    strncpy(&rc_error_message[0], resp->error_response.message, ERROR_MSG_LEN);
    return -1;
}

int
rc_ping(struct rc_client *client)
{
    struct rcrpc query, *resp;

    query.type = RCRPC_PING_REQUEST;
    query.len  = (uint32_t) RCRPC_PING_REQUEST_LEN;

    assert(!rc_net_send_rpc(&client->net, &query));
    assert(!rc_net_recv_rpc(&client->net, &resp));

    return rc_handle_errors(resp);
}

int
rc_write(struct rc_client *client,
         uint64_t table,
         uint64_t key,
         const char *buf,
         uint64_t len)
{
    assert(len <= MAX_DATA_WRITE_LEN);
    char query_buf[RCRPC_WRITE_REQUEST_LEN_WODATA + MAX_DATA_WRITE_LEN];
    struct rcrpc *query, *resp;
    query = (struct rcrpc *) query_buf;

    query->type = RCRPC_WRITE_REQUEST;
    query->len  = (uint32_t) RCRPC_WRITE_REQUEST_LEN_WODATA + len;
    query->write_request.table = table;
    query->write_request.key = key;
    query->write_request.buf_len = len;
    memcpy(query->write_request.buf, buf, len);

    assert(!rc_net_send_rpc(&client->net, query));
    assert(!rc_net_recv_rpc(&client->net, &resp));

    return rc_handle_errors(resp);
}

int
rc_insert(struct rc_client *client,
          uint64_t table,
          const char *buf,
          uint64_t len,
          uint64_t *key)
{
    assert(len <= MAX_DATA_WRITE_LEN);
    char query_buf[RCRPC_WRITE_REQUEST_LEN_WODATA + MAX_DATA_WRITE_LEN];
    struct rcrpc *query, *resp;
    query = (struct rcrpc *) query_buf;

    query->type = RCRPC_INSERT_REQUEST;
    query->len  = (uint32_t) RCRPC_INSERT_REQUEST_LEN_WODATA + len;
    query->insert_request.table = table;
    query->insert_request.buf_len = len;
    memcpy(query->insert_request.buf, buf, len);
    assert(!rc_net_send_rpc(&client->net, query));
    assert(!rc_net_recv_rpc(&client->net, &resp));
    *key = resp->insert_response.key;
    return 0;
}

int
rc_read(struct rc_client *client,
        uint64_t table,
        uint64_t key,
        char *buf,
        uint64_t *len)
{
    struct rcrpc query, *resp;

    query.type = RCRPC_READ_REQUEST;
    query.len  = (uint32_t) RCRPC_READ_REQUEST_LEN;
    query.read_request.table = table;
    query.read_request.key = key;
    assert(!rc_net_send_rpc(&client->net, &query));
    assert(!rc_net_recv_rpc(&client->net, &resp));
    int r = rc_handle_errors(resp);
    if (r)
        return r;

    *len = resp->read_response.buf_len;
    memcpy(buf, resp->read_response.buf, *len);

    return 0;
}

int
rc_create_table(struct rc_client *client, const char *name)
{
    struct rcrpc query, *resp;

    query.type = RCRPC_CREATE_TABLE_REQUEST;
    query.len  = (uint32_t) RCRPC_CREATE_TABLE_REQUEST_LEN;
    char *table_name = query.open_table_request.name;
    strncpy(table_name, name, sizeof(table_name));
    table_name[sizeof(table_name) - 1] = '\0';
    assert(!rc_net_send_rpc(&client->net, &query));
    assert(!rc_net_recv_rpc(&client->net, &resp));

    return rc_handle_errors(resp);
}

int
rc_open_table(struct rc_client *client, const char *name, uint64_t *table_id)
{
    struct rcrpc query, *resp;

    query.type = RCRPC_OPEN_TABLE_REQUEST;
    query.len  = (uint32_t) RCRPC_OPEN_TABLE_REQUEST_LEN;
    char *table_name = query.open_table_request.name;
    strncpy(table_name, name, sizeof(table_name));
    table_name[sizeof(table_name) - 1] = '\0';
    assert(!rc_net_send_rpc(&client->net, &query));
    assert(!rc_net_recv_rpc(&client->net, &resp));
    int r = rc_handle_errors(resp);
    if (r)
        return r;
    *table_id = resp->open_table_response.handle;

    return 0;
}

int
rc_drop_table(struct rc_client *client, const char *name)
{
    struct rcrpc query, *resp;

    query.type = RCRPC_DROP_TABLE_REQUEST;
    query.len  = (uint32_t) RCRPC_DROP_TABLE_REQUEST_LEN;
    char *table_name = query.open_table_request.name;
    strncpy(table_name, name, sizeof(table_name));
    table_name[sizeof(table_name) - 1] = '\0';
    assert(!rc_net_send_rpc(&client->net, &query));
    assert(!rc_net_recv_rpc(&client->net, &resp));

    return rc_handle_errors(resp);
}

int
rc_create_index(struct rc_client *client,
                uint64_t table_id,
                enum RCRPC_INDEX_TYPE type,
                bool unique, bool range_queryable,
                uint16_t *index_id)
{
    struct rcrpc query, *resp;

    query.type = RCRPC_CREATE_INDEX_REQUEST;
    query.len  = (uint32_t) RCRPC_CREATE_INDEX_REQUEST_LEN;
    query.create_index_request.table = table_id;
    query.create_index_request.type = (uint8_t) type;
    query.create_index_request.unique = unique;
    query.create_index_request.range_queryable = range_queryable;
    assert(!rc_net_send_rpc(&client->net, &query));
    assert(!rc_net_recv_rpc(&client->net, &resp));
    int r = rc_handle_errors(resp);
    if (r)
        return r;
    *index_id = resp->create_index_response.id;

    return 0;
}

int
rc_drop_index(struct rc_client *client, uint64_t table_id, uint16_t index_id)
{
    struct rcrpc query, *resp;

    query.type = RCRPC_DROP_INDEX_REQUEST;
    query.len  = (uint32_t) RCRPC_DROP_INDEX_REQUEST_LEN;
    query.drop_index_request.table = table_id;
    query.drop_index_request.id = index_id;
    assert(!rc_net_send_rpc(&client->net, &query));
    assert(!rc_net_recv_rpc(&client->net, &resp));
    return rc_handle_errors(resp);
}

struct rc_client *
rc_new() {
    return malloc(sizeof(struct rc_client *));
}

void
rc_free(struct rc_client *client)
{
    free(client);
}
