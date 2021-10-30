/*
 * param_server.c
 *
 *  Created on: Sep 29, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <csp/csp.h>
#include <csp/arch/csp_thread.h>
#include <csp/arch/csp_time.h>
#include <sys/types.h>

#include <param/param.h>
#include <param/param_queue.h>
#include <param/param_server.h>
#include <param/param_list.h>
#include <param/param_scheduler.h>
#include <param/param_commands.h>

struct param_serve_context {
	csp_packet_t * request;
	csp_packet_t * response;
	param_queue_t q_response;
};

static int __allocate(struct param_serve_context *ctx) {
	ctx->response = csp_buffer_get(PARAM_SERVER_MTU);
	if (ctx->response == NULL)
		return -1;
	param_queue_init(&ctx->q_response, &ctx->response->data[2], PARAM_SERVER_MTU, 0, PARAM_QUEUE_TYPE_SET, ctx->q_response.version);
	return 0;
}

static void __send(struct param_serve_context *ctx, int end) {
	if (ctx->q_response.version == 1) {
		ctx->response->data[0] = PARAM_PULL_RESPONSE;
	} else {
		ctx->response->data[0] = PARAM_PULL_RESPONSE_V2;
	}
	ctx->response->data[1] = (end) ? PARAM_FLAG_END : 0;
	ctx->response->length = ctx->q_response.used + 2;
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

	if (__allocate(&ctx) < 0) {
		csp_buffer_free(request);
		return;
	}

	if (all == 0) {

		/* Loop list in request */
		param_queue_t q_request;
		param_queue_init(&q_request, &ctx.request->data[2], ctx.request->length - 2, ctx.request->length - 2, PARAM_QUEUE_TYPE_SET, version);

		PARAM_QUEUE_FOREACH(param, reader, (&q_request), offset)
			if (param) {
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

static void param_serve_push(csp_packet_t * packet, int send_ack, int version)
{
	//csp_hex_dump("set handler", packet->data, packet->length);

	param_queue_t queue;
	param_queue_init(&queue, &packet->data[2], packet->length - 2, packet->length - 2, PARAM_QUEUE_TYPE_SET, version);
	int result = param_queue_apply(&queue);

	if ((result != 0) || (send_ack == 0)) {
		printf("Param serve push error, result = %d\n", result);
		csp_buffer_free(packet);
		return;
	}

	/* Send ack */
	packet->data[0] = PARAM_PUSH_RESPONSE;
	packet->data[1] = PARAM_FLAG_END;
	packet->length = 2;

	csp_sendto_reply(packet, packet, CSP_O_SAME);

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


