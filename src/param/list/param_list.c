/*
 * param_list.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libparam.h"
#ifdef PARAM_LIST_DYNAMIC
#include <malloc.h>
#endif

#include <csp/csp.h>
#include <sys/types.h>
#include <param/param.h>
#include <param/param_list.h>
#include <param/param_server.h>
#include <param/param_string.h>

#include <param/param_wildcard.h>
#include "param_list.h"


#ifdef PARAM_HAVE_SYS_QUEUE
#include <sys/queue.h>
#endif

#ifdef PARAM_HAVE_SYS_QUEUE
static SLIST_HEAD(param_list_head_s, param_s) param_list_head = {};
#endif

__attribute__((weak)) extern param_t *__start_param;
__attribute__((weak)) extern param_t *__stop_param;
uint8_t param_is_static(param_t * param) {

	if ((&__start_param != NULL) && (&__start_param != &__stop_param)) {
		if (param >= __start_param && param < __stop_param)
			return 1;
	}
	return 0;
}

param_t * param_list_iterate(param_list_iterator * iterator) {

	/* First element */
	if (iterator->element == NULL) {

		/* Static */
		if ((&__start_param != NULL) && (&__start_param != &__stop_param)) {
			iterator->phase = 0;
			iterator->element = __start_param;
			iterator->element_addr = &__start_param;
		} else {
			iterator->phase = 1;
#ifdef PARAM_HAVE_SYS_QUEUE
			iterator->element = SLIST_FIRST(&param_list_head);
#endif
		}
	} else {
		if(iterator->phase == 0){
			/* Increment in static memory */
			iterator->element_addr = iterator->element_addr + 1;
			iterator->element = *iterator->element_addr;
		}
#ifdef PARAM_HAVE_SYS_QUEUE
		else if(iterator->phase == 1){
			iterator->element = SLIST_NEXT((iterator->element), next);
		}
#endif
	}

	if(iterator->phase == 0){
		while(iterator->element_addr != &__stop_param){
			if (iterator->element->mask & PM_REMOTE && *iterator->element->node == 0){
				iterator->element_addr = iterator->element_addr + 1;
				iterator->element = *iterator->element_addr;
				continue;
			}
			return iterator->element;
		}

		iterator->phase = 1;
#ifdef PARAM_HAVE_SYS_QUEUE
		iterator->element = SLIST_FIRST(&param_list_head);
#else 
		iterator->element = NULL;
#endif
	}
	return iterator->element;
}

int param_list_add(param_t * item) {

	param_t * param;
	if ((param = param_list_find_id(*item->node, item->id)) != NULL) {

		/* To protect against updating local static params and ROM remote params
		   When creating remote dynamic params using the macro
		   strings are readonly. This can be recognized by checking if
		   the VMEM pointer is set */
		if (!param_is_static(param) && param->vmem != NULL && param != item) {
			param->mask = item->mask;
			param->type = item->type;
			param->array_size = item->array_size;
			param->array_step = item->array_step;

			if(param->name && item->name){
				strcpy(param->name, item->name);
			}
			if(param->unit && item->unit){
				strcpy(param->unit, item->unit);
			}
			if(param->docstr && item->docstr){
				strcpy(param->docstr, item->docstr);
			}
		}

		return 1;
	} else {
#ifdef PARAM_HAVE_SYS_QUEUE
		SLIST_INSERT_HEAD(&param_list_head, item, next);
#else
		return -1;
#endif
		return 0;
	}
}

#ifdef PARAM_HAVE_SYS_QUEUE
int param_list_remove(int node, uint8_t verbose) {

	int count = 0;

	param_list_iterator i = {};
	param_t * iter_param = param_list_iterate(&i);

	while (iter_param) {

		param_t * param = iter_param;  // Free the current parameter after we have used it to iterate.
		iter_param = param_list_iterate(&i);

		if (i.phase == 0)  // Protection against removing static parameters
			continue;

		uint8_t match = 0;

		if (node > 0)
			match = *param->node == node;

		if (match) {
			if (verbose)
				printf("Removing param: %s:%u[%d]\n", param->name, *param->node, param->array_size);
			// Using SLIST_REMOVE() means we iterate twice, but it is simpler.
			SLIST_REMOVE(&param_list_head, param, param_s, next);
			param_list_destroy(param);
			count++;
		}
	}

	return count;
}
void param_list_remove_specific(param_t * param, uint8_t verbose, int destroy) {

    if (verbose >= 2) {
        printf("Removing param: %s:%u[%d]\n", param->name, *param->node, param->array_size);
    }
    SLIST_REMOVE(&param_list_head, param, param_s, next);
    if (destroy) {
        param_list_destroy(param);
    }
}
#endif

param_t * param_list_find_id(int node, int id) {
	
	if (node < 0)
		node = 0;

	param_t * found = NULL;
	param_t * param;
	param_list_iterator i = {};

	while ((param = param_list_iterate(&i)) != NULL) {

		if (*param->node != node)
			continue;

		if (param->id == id) {
			found = param;
			break;
		}

		continue;
	}

	return found;
}

param_t * param_list_find_name(int node, const char * name) {
	
	if (node < 0 )
		node = 0;

	param_t * found = NULL;
	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {

		if (*param->node != node)
			continue;

		if (strcmp(param->name, name) == 0) {
			found = param;
			break;
		}

		continue;
	}

	return found;
}

void param_list_print(uint32_t mask, int node, const char * globstr, int verbosity) {
	param_t * param;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {
		if ((node >= 0) && (*param->node != node)) {
			continue;
		}
		if ((param->mask & mask) == 0) {
			continue;
		}
		if ((globstr != NULL) && strmatch(param->name, globstr, strlen(param->name), strlen(globstr)) == 0) {
			continue;
		}

		param_print(param, -1, NULL, 0, verbosity, 0);
		
	}
}

unsigned int param_list_packed_size(int list_version) {
	switch (list_version) {
		case 1: return sizeof(param_transfer_t);
		case 2: return sizeof(param_transfer2_t);
		case 3: return sizeof(param_transfer3_t);
		default: return 0;
	}
}

int param_list_pack(void* buf, int buf_size, int prio_only, int remote_only, int list_version) {

	param_t * param;
	int num_params = 0;

	void* param_packed = buf;
	param_list_iterator i = {};
	while ((param = param_list_iterate(&i)) != NULL) {
		if (prio_only && (param->mask & PM_PRIO_MASK) == 0)
			continue;

		if (remote_only && *param->node == 0)
			continue;

		if (list_version == 1) {
			
			// Not supported

		} else if (list_version == 2) {

			param_transfer2_t * rparam = param_packed;
			int node = *param->node;
			rparam->id = htobe16(param->id);
			rparam->node = htobe16(node);
			rparam->type = param->type;
			rparam->size = param->array_size;
			rparam->mask = htobe32(param->mask);
			
			strlcpy(rparam->name, param->name, sizeof(rparam->name));

			/* Ensure strings are null terminated */
			rparam->name[sizeof(rparam->name)-1] = '\0';

		} else {

			param_transfer3_t * rparam = param_packed;
			int node = *param->node;
			rparam->id = htobe16(param->id);
			rparam->node = htobe16(node);
			rparam->type = param->type;
			rparam->size = param->array_size;
			rparam->mask = htobe32(param->mask);
			
			strlcpy(rparam->name, param->name, sizeof(rparam->name));

			if (param->vmem) {
				rparam->storage_type = param->vmem->type;
			}

			if (param->unit != NULL) {
				strlcpy(rparam->unit, param->unit, sizeof(rparam->unit));
			}

			if (param->docstr != NULL) {
				strlcpy(rparam->help, param->docstr, sizeof(rparam->help));
			}

			/* Ensure strings are null terminated */
			rparam->name[sizeof(rparam->name)-1] = '\0';
			rparam->unit[sizeof(rparam->unit)-1] = '\0';
			rparam->help[sizeof(rparam->help)-1] = '\0';

		}
		
		param_packed += param_list_packed_size(list_version);
		num_params++;

		if (param_packed + param_list_packed_size(list_version) > buf + buf_size) {
			printf("Buffer size too small to hold parameters");
			break;
		}
	}

	return num_params;
	
}

#if defined PARAM_LIST_DYNAMIC && PARAM_LIST_POOL > 0
#error Dynamic and static param lists cannot co-exist
#endif

#if PARAM_LIST_POOL > 0

typedef struct param_heap_s {
	param_t param;
	vmem_t vmem;
	union {
		uint64_t alignme;
		uint8_t *buffer;
	};
	uint16_t node;
	uint32_t timestamp;
	char name[36];
	char unit[10];
	char help[150];
} param_heap_t;

static param_heap_t param_heap[PARAM_LIST_POOL] __attribute__ ((aligned (4))) __attribute__((section(".noinit")));
static uint32_t param_heap_used = 0;

/* Estimated average size of buffers */
static uint8_t param_buffer[PARAM_LIST_POOL * 16] __attribute__ ((aligned (4))) __attribute__((section(".noinit")));
static uint32_t param_buffer_used = 0;

static param_heap_t * param_list_alloc(int type, int array_size) {

	int buffer_required = param_typesize(type) * array_size;
	while(buffer_required%4 != 0) buffer_required++; /* Ensure that all values are word-aliged */

	if (param_heap_used >= PARAM_LIST_POOL || param_buffer_used + buffer_required > sizeof(param_buffer)) {
		return NULL;
	}

	param_heap_t* param = &param_heap[param_heap_used];
	memset(param, 0, sizeof(param_heap_t));
	param_heap_used++;

	param->buffer = &param_buffer[param_buffer_used];
	param_buffer_used += buffer_required;
	return param;
}

void param_list_clear() {

	SLIST_INIT(&param_list_head);
	param_heap_used = 0;
	param_buffer_used = 0;
}

/* WARNING: This function resets complete list */
static void param_list_destroy_impl(param_t * param) {

	param_heap_used = 0;
	param_buffer_used = 0;
}

#endif

#ifdef PARAM_LIST_DYNAMIC

void param_list_clear() {
	while (!SLIST_EMPTY(&param_list_head)) {
		struct param_s *param = SLIST_FIRST(&param_list_head);
		SLIST_REMOVE_HEAD(&param_list_head, next);
		param_list_destroy(param);
	}
}

typedef struct param_heap_s {
	param_t param;
	vmem_t vmem;
	union {
		uint64_t alignme;
		uint8_t *buffer;
	};
	uint16_t node;
	uint32_t timestamp;
	char name[36];
	char unit[10];
	char help[150];
} param_heap_t;

static param_heap_t * param_list_alloc(int type, int array_size) {

	param_heap_t * param_heap = calloc(1, sizeof(param_heap_t));
	if (param_heap == NULL) {
		return NULL;
	}
	param_heap->buffer = calloc(param_typesize(type), array_size);
	if (param_heap->buffer == NULL) {
		free(param_heap);
		return NULL;
	}

	return param_heap;
}

static void param_list_destroy_impl(param_t * param) {
	free(param->addr);
	free(param);
}
#endif

#if defined PARAM_LIST_DYNAMIC || PARAM_LIST_POOL > 0

int param_list_unpack(int node, void * data, int length, int list_version, int include_remotes) {

	uint16_t strlen;
	uint16_t addr;
	uint16_t id;
	uint8_t type;
	unsigned int size;
	uint32_t mask;
	int storage_type = VMEM_TYPE_UNKNOWN;
	char * name;
	char * unit;
	char * help;

	if (list_version == 1) {

		param_transfer_t * new_param = data;
		name = new_param->name;
		strlen = length - offsetof(param_transfer_t, name);
		if (strlen >= sizeof(param_transfer2_t) - offsetof(param_transfer2_t, name))
			strlen = sizeof(param_transfer2_t) - offsetof(param_transfer2_t, name) - 1;
		name[strlen] = '\0';
		addr = be16toh(new_param->id) >> 11;
		id = be16toh(new_param->id) & 0x7FF;
		type = new_param->type;
		size = new_param->size;
		mask = be32toh(new_param->mask) | PM_REMOTE;
		unit = NULL;
		help = NULL;

		/* Ensure strings are null terminated */
		name[sizeof(new_param->name)-1] = '\0';

	} else if (list_version == 2) {

		param_transfer2_t * new_param = data;
		name = new_param->name;
		strlen = length - offsetof(param_transfer2_t, name);
		if (strlen >= sizeof(param_transfer2_t) - offsetof(param_transfer2_t, name))
			strlen = sizeof(param_transfer2_t) - offsetof(param_transfer2_t, name) - 1;
		name[strlen] = '\0';
		addr = be16toh(new_param->node);
		id = be16toh(new_param->id);
		type = new_param->type;
		size = new_param->size;
		mask = be32toh(new_param->mask) | PM_REMOTE;
		unit = NULL;
		help = NULL;

		/* Ensure strings are null terminated */
		name[sizeof(new_param->name)-1] = '\0';

	} else {

		param_transfer3_t * new_param = data;
		name = new_param->name;
		addr = be16toh(new_param->node);
		id = be16toh(new_param->id);
		type = new_param->type;
		size = new_param->size;
		mask = be32toh(new_param->mask) | PM_REMOTE;
		storage_type = new_param->storage_type;
		unit = new_param->unit;
		help = new_param->help;

		/* Ensure strings are null terminated */
		name[sizeof(new_param->name)-1] = '\0';
		unit[sizeof(new_param->unit)-1] = '\0';
		help[sizeof(new_param->help)-1] = '\0';

	}

	if(!include_remotes && addr != 0) {
		return 1;
	}

	if (addr == 0)
		addr = node;

	if (size == 255)
		size = 1;

	//printf("Storage type %d\n", storage_type);

	param_t * param = param_list_create_remote(id, addr, type, mask, size, name, unit, help, storage_type);

	if (param != NULL) {
		printf("Got param: %s:%u[%d]\n", param->name, *param->node, param->array_size);

		/* Add to list */
		if (param_list_add(param) != 0)
			param_list_destroy(param);

		return 0;
	} else {
		return -1;
	}
}

int param_list_download(int node, int timeout, int list_version, int include_remotes) {

	/* Establish RDP connection */
	csp_conn_t * conn = csp_connect(CSP_PRIO_HIGH, node, PARAM_PORT_LIST, timeout, CSP_O_RDP | CSP_O_CRC32);
	if (conn == NULL)
		return -1;

	int count = 0;
	int count_remotes = 0;
	csp_packet_t * packet;
	while((packet = csp_read(conn, timeout)) != NULL) {

		//csp_hex_dump("Response", packet->data, packet->length);
		if ((count_remotes += param_list_unpack(node, packet->data, packet->length, list_version, include_remotes)) < 0) {
			csp_buffer_free(packet);
			break;
		}

		csp_buffer_free(packet);
		count++;
	}

	printf("Received %u parameters, of which %u remote parameters were skipped\n", count, count_remotes);
	csp_close(conn);

	return count;
}

void param_list_destroy(param_t * param) {
	param_list_destroy_impl(param);
}

param_t * param_list_create_remote(int id, int node, int type, uint32_t mask, int array_size, char * name, char * unit, char * help, int storage_type) {

	if (storage_type == 0xFFFF) {
		storage_type = VMEM_TYPE_UNKNOWN;
	}

	if (array_size < 1)
		array_size = 1;

	param_heap_t * param_heap = param_list_alloc(type, array_size);
	if (param_heap == NULL) {
		return NULL;
	}
	
	param_t * param = &param_heap->param;
	if (param == NULL) {
		return NULL;
	}

	param_heap->node = node;

	param->vmem = &param_heap->vmem;
	param->callback = NULL;
	param->name = param_heap->name;
	param->addr = param_heap->buffer;
	param->timestamp = &param_heap->timestamp;
	param->unit = param_heap->unit;
	param->docstr = param_heap->help;

	param->id = id;
	param->node = &param_heap->node;
	param->type = type;
	param->mask = mask;
	param->array_size = array_size;
	param->array_step = param_typesize(type);

	param->vmem->ack_with_pull = false;
	param->vmem->driver = NULL;
	param->vmem->name = "REMOTE";
	param->vmem->read = NULL;
	param->vmem->size = array_size*param_typesize(type);
	param->vmem->type = storage_type;
	param->vmem->vaddr = 0;
	param->vmem->backup = NULL;
	param->vmem->big_endian = false;
	param->vmem->restore = NULL;
	param->vmem->write = NULL;
	
	strlcpy(param->name, name, 36);
	if (unit) {
		strlcpy(param->unit, unit, 10);
	}
	if (help) {
		strlcpy(param->docstr, help, 150);
	}

	return param;

}
#endif
