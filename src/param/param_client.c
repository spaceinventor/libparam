/*
 * param_client.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <param/param_client.h>

#include <stdio.h>
#include <malloc.h>
#include <inttypes.h>
#include <param/param.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <csp/csp_hooks.h>
#include <sys/types.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_queue.h>

typedef void (*param_transaction_callback_f)(csp_packet_t *response, int verbose, int version, void * context);

static void param_transaction_callback_pull(csp_packet_t *response, int verbose, int version, void * context) {

	int from = response->id.src;
	//csp_hex_dump("pull response", response->data, response->length);
	//printf("From %d\n", from);

	param_queue_t queue;
	csp_timestamp_t time_now;
	csp_clock_get_time(&time_now);
	param_queue_init(&queue, &response->data[2], response->length - 2, response->length - 2, PARAM_QUEUE_TYPE_SET, version);
	queue.last_node = response->id.src;
	queue.client_timestamp = time_now.tv_sec;
	queue.last_timestamp = queue.client_timestamp;

	/* Write data to local memory */
	param_queue_apply(&queue, 0, from);

	if (verbose) {

		/* Loop over paramid's in pull response */
		mpack_reader_t reader;
		mpack_reader_init_data(&reader, queue.buffer, queue.used);
		while(reader.data < reader.end) {
			int id, node, offset = -1;
			long unsigned int timestamp = 0;
			param_deserialize_id(&reader, &id, &node, &timestamp, &offset, &queue);
			if (node == 0)
				node = from;
			param_t * param = param_list_find_id(node, id);

			/* We need to discard the data field, to get to next paramid */
			mpack_discard		(&reader);

			/* Print the local RAM copy of the remote parameter */
			if (param) {
				param_print(param, -1, NULL, 0, 0, 0);
			}

		}

	}

	csp_buffer_free(response);
}

int param_transaction(csp_packet_t *packet, int host, int timeout, param_transaction_callback_f callback, int verbose, int version, void * context) {

	//csp_hex_dump("transaction", packet->data, packet->length);

	/* Parameters can be setup with a special nodeid, which caused all transaction to be ignored
	   and return failure immediately */
	if (host == PARAM_REMOTE_NODE_IGNORE) {
		csp_buffer_free(packet);
		return -1;
	}

	csp_conn_t * conn = csp_connect(packet->id.pri, host, PARAM_PORT_SERVER, 0, CSP_O_CRC32);
	if (conn == NULL) {
		printf("param transaction failure\n");
		csp_buffer_free(packet);
		return -1;
	}

	csp_send(conn, packet);

	if (timeout == -1) {
		printf("param transaction failure\n");
		csp_close(conn);
		return -1;
	}

	int result = -1;
	while((packet = csp_read(conn, timeout)) != NULL) {

		int end = (packet->data[1] == PARAM_FLAG_END);

		//csp_hex_dump("response", packet->data, packet->length);

		if (callback) {
			callback(packet, verbose, version, context);
		} else {
			csp_buffer_free(packet);
		}

		if (end) {
			result = 0;
			break;
		}

	}

	//printf("Successful param transaction, result: %d\n", result);
	csp_close(conn);
	return result;
}

int param_pull_all(uint8_t prio, int verbose, int host, uint32_t include_mask, uint32_t exclude_mask, int timeout, int version) {

	csp_packet_t *packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;
	if (version == 2) {
		packet->data[0] = PARAM_PULL_ALL_REQUEST_V2;
	} else {
		packet->data[0] = PARAM_PULL_ALL_REQUEST;
	}
	packet->data[1] = 0;
	packet->data32[1] = htobe32(include_mask);
	packet->data32[2] = htobe32(exclude_mask);
	packet->length = 12;
	packet->id.pri = prio;
	return param_transaction(packet, host, timeout, param_transaction_callback_pull, verbose, version, NULL);

}

int param_pull_queue(param_queue_t *queue, uint8_t prio, int verbose, int host, int timeout) {

	if ((queue == NULL) || (queue->used == 0))
		return 0;

	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	if (queue->version == 2) {
		packet->data[0] = PARAM_PULL_REQUEST_V2;
	} else {
		packet->data[0] = PARAM_PULL_REQUEST;
	}

	packet->data[1] = 0;

	memcpy(&packet->data[2], queue->buffer, queue->used);

	packet->length = queue->used + 2;
	packet->id.pri = prio;
	return param_transaction(packet, host, timeout, param_transaction_callback_pull, verbose, queue->version, NULL);

}


int param_pull_single(param_t *param, int offset, uint8_t prio, int verbose, int host, int timeout, int version) {

	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -1;

	if (version == 2) {
		packet->data[0] = PARAM_PULL_REQUEST_V2;
	} else {
		packet->data[0] = PARAM_PULL_REQUEST;
	}
	packet->data[1] = 0;

	param_queue_t queue;
	param_queue_init(&queue, &packet->data[2], PARAM_SERVER_MTU - 2, 0, PARAM_QUEUE_TYPE_GET, version);
	param_queue_add(&queue, param, offset, NULL);

	packet->length = queue.used + 2;
	packet->id.pri = prio;
	return param_transaction(packet, host, timeout, param_transaction_callback_pull, verbose, version, NULL);
}


int param_push_queue(param_queue_t *queue, int verbose, int host, int timeout, uint32_t hwid, bool ack_with_pull) {

	if ((queue == NULL) || (queue->used == 0))
		return 0;

	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -2;

	if (queue->version == 2) {
		packet->data[0] = PARAM_PUSH_REQUEST_V2;
	} else {
		packet->data[0] = PARAM_PUSH_REQUEST;
	}

	packet->data[1] = 0;
	param_transaction_callback_f cb = NULL;

	if (ack_with_pull) {
		packet->data[1] = 1;
		cb = param_transaction_callback_pull;
	}

	memcpy(&packet->data[2], queue->buffer, queue->used);

	packet->length = queue->used + 2;
	packet->id.pri = CSP_PRIO_HIGH;

	/* Append hwid, no care given to endian at this point */
	if (hwid > 0) {
		packet->data[0] = PARAM_PUSH_REQUEST_V2_HWID;
		//printf("Add hwid %x\n", hwid);
		//csp_hex_dump("tx", packet->data, packet->length);
		memcpy(&packet->data[packet->length], &hwid, sizeof(hwid));
		packet->length += sizeof(hwid);

	}

	int result = param_transaction(packet, host, timeout, cb, verbose, queue->version, NULL);

	if (result < 0) {
		printf("push queue error\n");
		return -1;
	}
	if (verbose) {
		//printf("  ACK from %d\n", host);
		// param_queue_print(queue);
	}
	if(!ack_with_pull){
		param_queue_apply(queue, 0, host);
	}

	return 0;
}

int param_push_single(param_t *param, int offset, void *value, int verbose, int host, int timeout, int version, bool ack_with_pull) {

	csp_packet_t * packet = csp_buffer_get(PARAM_SERVER_MTU);
	if (packet == NULL)
		return -1;

	packet->data[1] = 0;
	param_transaction_callback_f cb = NULL;

	if (ack_with_pull) {
		packet->data[1] = 1;
		cb = param_transaction_callback_pull;
	}

	if(version == 2) {
		packet->data[0] = PARAM_PUSH_REQUEST_V2;
	} else {
		packet->data[0] = PARAM_PUSH_REQUEST;
	}

	param_queue_t queue;
	param_queue_init(&queue, &packet->data[2], PARAM_SERVER_MTU - 2, 0, PARAM_QUEUE_TYPE_SET, version);
	param_queue_add(&queue, param, offset, value);

	packet->length = queue.used + 2;
	packet->id.pri = CSP_PRIO_HIGH;
	int result = param_transaction(packet, host, timeout, cb, verbose, version, NULL);

	if (result < 0) {
		return -1;
	}

	/* If it was a remote parameter, set the value after the ack but not if ack with push which sets param timestamp */
	if (param->node != 0 && value != NULL && *param->timestamp == 0)
	{
		if (offset < 0) {
			for (int i = 0; i < param->array_size; i++)
				param_set(param, i, value);
		} else {
			param_set(param, offset, value);
		}
	}

	return 0;
}

