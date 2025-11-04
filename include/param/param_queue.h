/*
 * param_queue.h
 *
 *  Created on: Nov 9, 2017
 *      Author: johan
 */

#ifndef LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_
#define LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_

#include <param/param.h>
#include <param/param_list.h>
#include <mpack/mpack.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	PARAM_QUEUE_TYPE_GET,
	PARAM_QUEUE_TYPE_SET,
	PARAM_QUEUE_TYPE_EMPTY,
} param_queue_type_e;

typedef struct param_queue_s {
	char *buffer;
	uint16_t buffer_size;
	uint16_t used;
	uint8_t version;
	uint8_t type;
	char name[20];

	/* State used by serializer */
	uint16_t last_node;
	csp_timestamp_t last_timestamp;
	csp_timestamp_t client_timestamp;
} param_queue_t;

/**
 * @brief Control the amount of prints that the param_queue_* functions will output
 */
extern uint32_t param_queue_dbg_level;

void param_queue_init(param_queue_t * queue, void * buffer, int buffer_size, int used, param_queue_type_e type, int version);

int param_queue_add(param_queue_t *queue, param_t *param, int offset, void *value);

/* Default callback for param decoding errors (in `param_queue_apply()`).
	Can be called by a custom callback, if they also want a print. */
void param_decode_err_dbg_print(uint16_t node, uint16_t id, uint8_t severity, void * context);

typedef void (*param_decode_err_callback_f)(uint16_t node, uint16_t id, uint8_t severity, void * context);
/**
 * @brief Same as `param_queue_apply()`, but with a (user-provided) contextualized callback whenever a parameter decoding error is encountered.
 * 
 * @param queue[in]				Pointer to queue
 * @param host[in]              If host is set and the node is 0, it will be set to host
 * @param err_callback[in]		Callback function to call whenever a decoding error is encountered.
 * @param err_context[in] 		User-supplied context for the callback function.
 * @return int 					0 OK, -1 ERROR
 */
int param_queue_apply_err_callback(param_queue_t *queue, int host, param_decode_err_callback_f err_callback, void * err_context);

/**
 * @brief 						Applies the content of a queue to memory.
 * @param queue[in]				Pointer to queue
 * @param host[in]              If host is set and the node is 0, it will be set to host
 * @return 						0 OK, -1 ERROR
 */
int param_queue_apply(param_queue_t *queue, int host);

void param_queue_print(param_queue_t *queue);
void param_queue_print_local(param_queue_t *queue);
void param_queue_print_params(param_queue_t *queue, uint32_t ref_timestamp);

typedef int (*param_queue_callback_f)(void * context, param_queue_t *queue, param_t * param, int offset, void *reader);
int param_queue_foreach(param_queue_t *queue, param_queue_callback_f callback, void * context);

#ifdef __cplusplus
}
#endif

#endif /* LIB_PARAM_SRC_PARAM_PARAM_QUEUE_H_ */
