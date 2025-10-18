/*
 * param_server.c
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <csp/csp.h>
#include <csp/arch/csp_time.h>
#include <sys/types.h>

#include <param/param.h>
#include <param/param_queue.h>
#include <param/param_server.h>
#include <param/param_list.h>
#include <param/param_serializer.h>
#ifdef PARAM_HAVE_SCHEDULER
#include <param/param_scheduler.h>
#endif
#ifdef PARAM_HAVE_COMMANDS
#include <param/param_commands.h>
#endif

struct param_serve_context {
	csp_packet_t * request;
	csp_packet_t * response;
	param_queue_t q_response;
	csp_conn_t * publish_conn;
};

static int __allocate(struct param_serve_context *ctx) {
	ctx->response = csp_buffer_get(PARAM_SERVER_MTU);
	if (ctx->response == NULL)
		return -1;
	param_queue_init(&ctx->q_response, &ctx->response->data[2], PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, ctx->q_response.version);
	return 0;
}

static void __send(struct param_serve_context *ctx, int end) {

	ctx->response->data[1] = (end) ? PARAM_FLAG_END : 0;
	ctx->response->length = ctx->q_response.used + 2;

	if (ctx->q_response.version == 1) {
		ctx->response->data[0] = PARAM_PULL_RESPONSE;
	} else {
		ctx->response->data[0] = PARAM_PULL_RESPONSE_V2;
	}
	ctx->response->data[1] = (end) ? PARAM_FLAG_END : 0;
	ctx->response->length = ctx->q_response.used + 2;

	if (ctx->publish_conn != NULL) {
		ctx->response->data[1] |= PARAM_FLAG_NOACK;

		ctx->response->id.flags = CSP_O_CRC32;
		ctx->response->id.src = 0;
	
		if (ctx->publish_conn == NULL) {
			printf("param transaction failure\n");
			return;
		}
	
		csp_send(ctx->publish_conn, ctx->response);
		return;
	}

	csp_sendto_reply(ctx->request, ctx->response, CSP_O_SAME);
}

static int __add(struct param_serve_context *ctx, param_t * param, int offset) {

	int result = param_queue_add(&ctx->q_response, param, offset, NULL);
	if (result != 0) {

		/* Flush */
		__send(ctx, 0);
		if (__allocate(ctx) < 0)
			return -1;

		/* Retry on fresh buffer */
		if (param_queue_add(&ctx->q_response, param, offset, NULL) != 0) {
			printf("warn: param too big for mtu\n");
		}
	}
	return  0;
}

static void param_serve_pull_request(csp_packet_t * request, int all, int version) {

	struct param_serve_context ctx;
	ctx.request = request;
	ctx.q_response.version = version;
	/* If packet->data[1] == 1 ack with pull response */
	int ack_with_pull = request->data[1] == 1 ? 1 : 0;
	ctx.publish_conn = NULL;

	if (__allocate(&ctx) < 0) {
		csp_buffer_free(request);
		return;
	}

	if (all == 0) {

		/* Loop list in request */
		param_queue_t q_request;
		param_queue_init(&q_request, &ctx.request->data[2], ctx.request->length - 2, ctx.request->length - 2, PARAM_QUEUE_TYPE_SET, version);

		mpack_reader_t reader;
		mpack_reader_init_data(&reader, q_request.buffer, q_request.used);

		while(reader.data < reader.end) {
			int id, node, offset = -1;
			csp_timestamp_t timestamp = { .tv_sec = 0, .tv_nsec = 0 };
			param_deserialize_id(&reader, &id, &node, &timestamp, &offset, &q_request);
			param_t * param = param_list_find_id(node, id);
			if (param) {
				if(ack_with_pull) {
					/* Move reader forward to skip values as we normally use a get queue */
					mpack_discard(&reader);

					/* Do not ack queues with duplicate parameters multiple times */
					mpack_reader_t _reader;
					mpack_reader_init_data(&_reader, ctx.q_response.buffer, ctx.q_response.used);
					int found = 0;
					while(_reader.data < _reader.end) {
						int _id, _node, _offset = -1;
						csp_timestamp_t _timestamp = { .tv_sec = 0, .tv_nsec = 0 };
						param_deserialize_id(&_reader, &_id, &_node, &_timestamp, &_offset, &ctx.q_response);
						param_t * _param = param_list_find_id(_node, _id);

						/* Move reader forward to skip values */
						mpack_discard(&_reader);

						if(_param == param){
							found = 1;	
							break;
						}
					}

					if(found){
						continue;
					}

					/* If vmem is not flagged to allow ack with pull skip reading values */
					if(param->vmem && !param->vmem->ack_with_pull) {
						continue;
					}

					/* Set offset to -1 to ack with all array values */
					offset = -1;
				} 
				if (__add(&ctx, param, offset) < 0) {
					csp_buffer_free(request);
					return;
				}
			}
		}

	} else {

		/* Loop the full parameter list */
		param_t * param;
		param_list_iterator i = {};
		while ((param = param_list_iterate(&i)) != NULL) {
			if (param->mask & PM_HIDDEN) {
				continue;
			}
			uint32_t include_mask = be32toh(ctx.request->data32[1]);
			uint32_t exclude_mask = 0x00000000;
			if (version >= 2) {
			    exclude_mask = be32toh(ctx.request->data32[2]);
			}

			/* If none of the include matches, continue */
			if ((param->mask & include_mask) == 0)
				continue;

			/* In any one of the exclude matches, continue */
			if ((param->mask & exclude_mask) != 0)
				continue;
			
			if (__add(&ctx, param, -1) < 0) {
				csp_buffer_free(request);
				return;
			}
		}
	}

	__send(&ctx, 1);

	csp_buffer_free(request);

}

/**
 * @brief Apply a parameter queue parsed from the provided packet.
 *
 * @param packet Packet containing the parameter queue.
 */
static void param_serve_push(csp_packet_t * packet, int send_ack, int version) {

	//csp_hex_dump("set handler", packet->data, packet->length);

	param_queue_t queue;
	param_queue_init(&queue, &packet->data[2], packet->length - 2, packet->length - 2, PARAM_QUEUE_TYPE_SET, version);
	int result = param_queue_apply(&queue, 0);

	if ((result != 0) || (send_ack == 0)) {
		if (result != 0) {
			printf("Param serve push error, result = %d\n", result);
		}
		csp_buffer_free(packet);
		return;
	}

	if (packet->data[1] & PARAM_FLAG_NOACK) {
		csp_buffer_free(packet);
	} else if (packet->data[1] & PARAM_FLAG_PULLWITHACK) {
		param_serve_pull_request(packet, 0, 2);
	} else {
		/* Send ack */
		packet->data[0] = PARAM_PUSH_RESPONSE;
		packet->data[1] = PARAM_FLAG_END;
		packet->length = 2;
		csp_sendto_reply(packet, packet, CSP_O_SAME);
	}
}


void param_serve(csp_packet_t * packet) {
	switch(packet->data[0]) {
		case PARAM_PULL_REQUEST:
			param_serve_pull_request(packet, 0, 1);
			break;
		case PARAM_PULL_REQUEST_V2:
		    param_serve_pull_request(packet, 0, 2);
		    break;

		case PARAM_PULL_ALL_REQUEST:
			param_serve_pull_request(packet, 1, 1);
			break;
		case PARAM_PULL_ALL_REQUEST_V2:
			param_serve_pull_request(packet, 1, 2);
			break;

		case PARAM_PULL_RESPONSE:
			param_serve_push(packet, 0, 1);
			break;
		case PARAM_PULL_RESPONSE_V2:
			param_serve_push(packet, 0, 2);
			break;

		case PARAM_PUSH_REQUEST:
			param_serve_push(packet, 1, 1);
			break;
		case PARAM_PUSH_REQUEST_V2:
			param_serve_push(packet, 1, 2);
			break;
		case PARAM_PUSH_REQUEST_V2_HWID: {

			/* Strip hwid off the end */
			uint32_t hwid;
			memcpy(&hwid, &packet->data[packet->length-sizeof(uint32_t)], sizeof(uint32_t));
			packet->length -= sizeof(uint32_t);

			//printf("hwid %d\n", hwid);
			__attribute__((weak)) int serial_get(void);
			if ((serial_get != NULL && hwid != serial_get()) && (hwid != 1234)) {
				printf("hwid did not match\n");
				csp_buffer_free(packet);
				break;
			}

			param_serve_push(packet, 1, 2);

			break;
		}


#ifdef PARAM_HAVE_SCHEDULER

		case PARAM_SCHEDULE_PUSH:
			param_serve_schedule_push(packet);
			break;

		/*
		case PARAM_SCHEDULE_PULL:
			param_serve_schedule_pull(packet);
			break;
			*/

		case PARAM_SCHEDULE_SHOW_REQUEST:
			param_serve_schedule_show(packet);
			break;

		case PARAM_SCHEDULE_LIST_REQUEST:
			param_serve_schedule_list(packet);
			break;

		case PARAM_SCHEDULE_RM_REQUEST:
			param_serve_schedule_rm_single(packet);
			break;

		case PARAM_SCHEDULE_RM_ALL_REQUEST:
			param_serve_schedule_rm_all(packet);
			break;

		case PARAM_SCHEDULE_RESET_REQUEST:
			param_serve_schedule_reset(packet);
			break;

	#ifdef PARAM_HAVE_COMMANDS
		case PARAM_SCHEDULE_COMMAND_REQUEST:
			param_serve_schedule_command(packet);
			break;

	#endif
#endif

#ifdef PARAM_HAVE_COMMANDS

		case PARAM_COMMAND_ADD_REQUEST:
			param_serve_command_add(packet);
			break;
			
		case PARAM_COMMAND_SHOW_REQUEST:
			param_serve_command_show(packet);
			break;

		case PARAM_COMMAND_LIST_REQUEST:
			param_serve_command_list(packet);
			break;

		case PARAM_COMMAND_RM_REQUEST:
			param_serve_command_rm_single(packet);
			break;

		case PARAM_COMMAND_RM_ALL_REQUEST:
			param_serve_command_rm_all(packet);
			break;
		
#endif

		default:
			printf("Unknown parameter request\n");
			csp_buffer_free(packet);
			break;
	}


}

#if PARAM_NUM_PUBLISHQUEUES > 0

static uint16_t param_publish_periodicity[PARAM_NUM_PUBLISHQUEUES];
static uint16_t param_publish_destination[PARAM_NUM_PUBLISHQUEUES];
static csp_prio_t param_publish_priority[PARAM_NUM_PUBLISHQUEUES];
static param_shall_publish_t param_shall_publish;
static uint32_t last_periodic;

static struct param_serve_context param_publish_ctx[PARAM_NUM_PUBLISHQUEUES];

__attribute__((weak)) extern param_publish_t * __start_param_publish;
__attribute__((weak)) extern param_publish_t * __stop_param_publish;

void param_publish_periodic() {

	uint32_t timestamp = csp_get_ms();
	uint32_t increment = timestamp - last_periodic; // Implicitely handling timestamp wraparounds
	last_periodic = timestamp;

	static int16_t param_publish_countdown[PARAM_NUM_PUBLISHQUEUES];

	for (int q = 0; q < PARAM_NUM_PUBLISHQUEUES; q++) {

		if (param_publish_ctx[q].publish_conn == NULL) {
			continue;
		}

		if (param_publish_countdown[q] > increment) {
			param_publish_countdown[q] -= increment;
			continue;
		}

		param_publish_countdown[q] = param_publish_periodicity[q];

		if (param_shall_publish && !param_shall_publish(q)) {
			continue;
		}

		struct param_serve_context * ctx = &param_publish_ctx[q];

		if (__allocate(ctx) < 0) {
			printf("Cannot allocate buffer for push queue\n");
			return;
		}
	
		param_queue_t publishqueue;
		param_queue_init(&publishqueue, NULL, PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, 2);

		for (int i = 0; i < (&__stop_param_publish - &__start_param_publish); i++) {
			param_publish_t * p = &__start_param_publish[i];
			if (p->queue == q) {
				__add(ctx, p->param, -1);
			}
		}
		__send(ctx, 1);
	}
	return;
}

void param_publish_configure(param_publish_id_t queueid, uint16_t destination, uint16_t periodicity_ms, csp_prio_t csp_prio) {

    param_publish_destination[queueid] = destination;
    param_publish_periodicity[queueid] = periodicity_ms;
    param_publish_priority[queueid] = csp_prio;
}

void param_publish_init(param_shall_publish_t shall_publish) {

	param_shall_publish = shall_publish;

	if ((__start_param_publish == NULL) || (__start_param_publish == __stop_param_publish)) {
		return;
	}

	for (int q = 0; q < PARAM_NUM_PUBLISHQUEUES; q++) {
		if (param_publish_destination[q] == 0 || param_publish_periodicity[q] == 0) {
			continue;
		}
		param_publish_ctx[q].publish_conn = csp_connect(param_publish_priority[q], param_publish_destination[q], PARAM_PORT_SERVER, 0, CSP_O_CRC32);
		param_publish_ctx[q].q_response.version = 2;
	}

	last_periodic = csp_get_ms();
}
#endif
