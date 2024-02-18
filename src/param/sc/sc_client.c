#include <sc/sc_client.h>
#include <param/param_server.h>

typedef void (*param_transaction_callback_f)(csp_packet_t *response, int verbose, int version);
int param_transaction(csp_packet_t *packet, int host, int timeout, param_transaction_callback_f callback, int verbose, int version, void * context);

int sc_cmd_upload_client(param_queue_t* queue, uint16_t server, unsigned int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet == NULL)
        return -2;

    if (queue->version == 2) {
        packet->data[0] = PARAM_COMMAND_UPLOAD_REQUEST_V2;
    } else {
        return -3;
    }

    packet->data[1] = PARAM_FLAG_END;

    param_cmd_upload_t* cmd = (param_cmd_upload_t*)&packet->data[2];

    memcpy(&cmd->param_queue, queue, sizeof(*queue));
    memcpy(&cmd->param_buffer, queue->buffer, queue->used);

    cmd->execute = 0;
    cmd->store = 1;

	packet->length = 2 + sizeof(*cmd) + queue->used;
	int result = param_transaction(packet, server, timeout, NULL, 0, queue->version, NULL);

    return result;
}

void sc_cmd_list_client_cb(csp_packet_t *response, int verbose, int version) {

    printf("Showing commands in server with node %d:\n", response->id.src);

    uint32_t unpacked_len = 2;
    while (unpacked_len < response->length) {
        param_sc_rsp_t* rsp_element = (param_sc_rsp_t*)&response->data[unpacked_len];
        unpacked_len += sizeof(param_sc_rsp_t);
        printf("HASH 0x%X\n", rsp_element->hash);
        if (rsp_element->result < 0) {
            printf("CMD was found corrupted, thus removed from database\n");
        }
    }
}

int sc_cmd_list_client(uint16_t server, unsigned int timeout) {
    
    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet == NULL)
        return -2;

    packet->data[0] = PARAM_COMMAND_LIST_REQUEST_V2;
    packet->data[1] = PARAM_FLAG_END;
    packet->length = 2;

	int result = param_transaction(packet, server, timeout, sc_cmd_list_client_cb, 0, 2, NULL);

    return result;
}

void sc_cmd_download_cb(csp_packet_t *response, int verbose, int version) {

    printf("Showing downloaded command(s):\n");
    uint32_t unpacked_len = 2;
    while (unpacked_len < response->length) {
        param_cmd_download_t* rsp_element = (param_cmd_download_t*)&response->data[unpacked_len];
        rsp_element->param_queue.buffer = rsp_element->param_buffer;
        unpacked_len += sizeof(param_cmd_download_t) + rsp_element->param_queue.used;

        param_queue_print(&rsp_element->param_queue);
    }
}

int sc_cmd_download_client(param_hash_t hash, uint16_t server, unsigned int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet == NULL)
        return -2;

    packet->data[0] = PARAM_COMMAND_DOWNLOAD_REQUEST_V2;
    packet->data[1] = PARAM_FLAG_END;
    packet->length = 2;

    param_sc_req_t* cmd = (param_sc_req_t*)&packet->data[packet->length];
    packet->length += sizeof(*cmd);

    cmd->hash = hash;
    
	int result = param_transaction(packet, server, timeout, sc_cmd_download_cb, 0, 2, NULL);

    return result;
}

int sc_cmd_remove_client(param_hash_t hash, uint16_t server, unsigned int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet == NULL)
        return -2;

    packet->data[0] = PARAM_COMMAND_REMOVE_REQUEST_V2;
    packet->data[1] = PARAM_FLAG_END;
    packet->length = 2;

    param_sc_req_t* cmd = (param_sc_req_t*)&packet->data[2];
    packet->length += sizeof(*cmd);

    cmd->hash = hash;

	int result = param_transaction(packet, server, timeout, NULL, 0, 2, NULL);

    return result;
}

int sc_sch_push_client(param_queue_t* queue, uint32_t time, uint16_t latency_buffer_s, uint16_t server, unsigned int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet == NULL)
        return -2;

    if (queue->version == 2) {
        packet->data[0] = PARAM_SCHEDULE_PUSH_REQUEST_V2;
    } else {
        return -3;
    }

    packet->data[1] = PARAM_FLAG_END;

    param_sch_push_t* cmd = (param_sch_push_t*)&packet->data[2];

    cmd->timestamp_s = time;
    cmd->latency_buffer_s = latency_buffer_s;
    memcpy(&cmd->param_queue, queue, sizeof(*queue));
    memcpy(&cmd->param_buffer, queue->buffer, queue->used);

	packet->length = 2 + sizeof(*cmd) + queue->used;
	int result = param_transaction(packet, server, timeout, NULL, 0, queue->version, NULL);

    return result;
}

int sc_sch_cmd_client(param_hash_t cmd_hash, uint32_t time, uint16_t latency_buffer_s, uint16_t server, unsigned int timeout) {

    csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
    if (packet == NULL)
        return -2;

    packet->data[0] = PARAM_SCHEDULE_COMMAND_REQUEST_V2;
    packet->data[1] = PARAM_FLAG_END;

    param_sch_command_t* cmd = (param_sch_command_t*)&packet->data[2];

    cmd->timestamp_s = time;
    cmd->latency_buffer_s = latency_buffer_s;
    cmd->command_hash = cmd_hash;

	packet->length = 2 + sizeof(*cmd);

	int result = param_transaction(packet, server, timeout, NULL, 0, 2, NULL);

    return result;
}
