/*
 * param_list.c
 *
 *  Created on: Oct 9, 2016
 *      Author: johan
 */

#include <stdio.h>
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

#include "../param_wildcard.h"
#include "param_list.h"


#ifdef PARAM_HAVE_SYS_QUEUE
#include <sys/queue.h>
#endif

static bool param_list_initialized = false;
static param_t * param_head = 0;

PARAM_SECTION_INIT_NO_FUNC(param)

/** 
 * List implementation independent on sys/queue.h
*/

param_t * param_list_insert(param_t * head, param_t * param) {

    /* param->next: first entry  prev: none */
    param->next = head;
    param_t * prev = 0;

    /* As long af param->next is alphabetically lower than cmd */
    while (param->next && (strcmp(param->name, param->next->name) > 0)) {
        /* cmd->next: next entry  prev: previous entry */
        prev = param->next;
        param->next = param->next->next;
    }

    if (prev) {
        /* Insert before param->next */
        prev->next = param;
    } else {
        /* Insert as first entry */
        head = param;
    }

    return head;
}


int param_list_add(param_t * item) {

	param_t * param;
	if ((param = param_list_find_id(item->node, item->id)) != NULL) {

		/* To protect against updating local static params */
		if (param->alloc) {
			param->mask = item->mask;
			param->type = item->type;
			param->array_size = item->array_size;
			param->array_step = item->array_step;

			strcpy(param->name, item->name);
			strcpy(param->unit, item->unit);
			strcpy(param->docstr, item->docstr);
		}

		return 1;
	} else {
        param_head = param_list_insert(param_head, item);
		return 0;
	}

}

param_t * param_list_add_section(param_t * head, param_t * start, param_t *stop)
{

	for (param_t * param = start; param < stop; ++param) {
        head = param_list_insert(head, param);
	}

    return head;
}

param_t * param_list_head() {

    if (!param_list_initialized) {
        param_list_initialized = true;
        param_head = param_list_add_section(0, param_section_start, param_section_stop);
    }

    return param_head;
}

param_t * param_list_iterate(param_t * param) {

    return param ? param->next : 0;
}

param_t * param_list_find_param(param_t * p) {
	
    for (param_t * param = param_list_head(); param; param = param_list_iterate(param)) {

		if (param == p) {
			return param;
		}

    }

	return 0;
}

param_t * param_list_find_id(int node, int id) {
	
	if (node < 0)
		node = 0;

    for (param_t * param = param_list_head(); param; param = param_list_iterate(param)) {

		if ((param->node == node) && (param->id == id)) {
			return param;
		}

    }

	return 0;
}

param_t * param_list_find_name(int node, char * name) {
	
	if (node < 0 )
		node = 0;

    for (param_t * param = param_list_head(); param; param = param_list_iterate(param)) {

		if ((param->node == node) && (strcmp(param->name, name) == 0)) {
			return param;
		}

    }

	return 0;
}

void param_list_print(uint32_t mask, int node, char * globstr, int verbosity) {

	if (node < 0 )
		node = 0;

    for (param_t * param = param_list_head(); param; param = param_list_iterate(param)) {

		if ((node >= 0) && (param->node != node)) {
			continue;
		}
		if ((param->mask & mask) == 0) {
			continue;
		}
		if ((globstr != NULL) && strmatch(param->name, globstr, strlen(param->name), strlen(globstr)) == 0) {
			continue;
		}

		param_print(param, -1, NULL, 0, verbosity);

	}
}

int param_list_remove(int node, uint8_t verbose) {

    if (node < 0) {
        return 0;
    }

	int count = 0;

    param_t * prev = 0;
    for (param_t * param = param_list_head(); param; param = param_list_iterate(param)) {

		if ((param->alloc == 0) || (param->node != node)) {
            prev = param;
            continue;
        }

        /* Remove param from list */
        prev->next = param->next;
        param->next = 0;

        param_list_destroy(param);

        count++;
	}

	return count;
}

int param_list_remove_specific(param_t * param, uint8_t verbose, int destroy) {

    /* Find parameter and previous struct */
    param_t * prev = 0;
    param_t * p = param_list_head();
    for (; p && (p != param); p = param_list_iterate(p)) {
        prev = p;
    }

    if (!p) {
        return 0;
    }

    if (verbose)
        printf("Removing param: %s:%u[%d]\n", param->name, param->node, param->array_size);

    /* Remove param from list */
    prev->next = param->next;
    param->next = 0;

    if (destroy)
        param_list_destroy(param);

    return 1;
}

unsigned int param_list_packed_size(int list_version) {
	switch (list_version) {
		case 1: return sizeof(param_transfer_t);
		case 2: return sizeof(param_transfer2_t);
		case 3: return sizeof(param_transfer3_t);
		default: return 0;
	}
}

int param_list_unpack(int node, void * data, int length, int list_version, int include_remotes) {

	uint16_t strlen;
	uint16_t addr;
	uint16_t id;
	uint8_t type;
	unsigned int size;
	uint32_t mask;
	uint16_t storage_type = -1;
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

	if (addr == 0)
		addr = node;

	if (size == 255)
		size = 1;

	if(!include_remotes && node != addr) {
		return 1;
	}

	//printf("Storage type %d\n", storage_type);

	param_t * param = param_list_create_remote(id, addr, type, mask, size, name, unit, help, storage_type);

	if (param != NULL) {
		printf("Got param: %s:%u[%d]\n", param->name, param->node, param->array_size);

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

int param_list_pack(void* buf, int buf_size, int prio_only, int remote_only, int list_version) {

	int num_params = 0;

	void* param_packed = buf;
    for (param_t * param = param_list_head(); param; param = param_list_iterate(param)) {

		if (prio_only && (param->mask & PM_PRIO_MASK) == 0)
			continue;

		if (remote_only && param->node == 0)
			continue;

		if (list_version == 1) {
			
			// Not supported

		} else if (list_version == 2) {

			param_transfer2_t * rparam = param_packed;
			int node = param->node;
			rparam->id = htobe16(param->id);
			rparam->node = htobe16(node);
			rparam->type = param->type;
			rparam->size = param->array_size;
			rparam->mask = htobe32(param->mask);
			
			strlcpy(rparam->name, param->name, sizeof(rparam->name));

		} else {

			param_transfer3_t * rparam = param_packed;
			int node = param->node;
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
	union {
		uint64_t alignme;
		uint8_t *buffer;
	};
	uint32_t timestamp;
	char name[36];
	char unit[10];
	char help[150];
} param_heap_t;

static param_heap_t param_heap[PARAM_LIST_POOL]  __attribute__((section(".noinit")));
static uint32_t param_heap_used = 0;
static uint8_t param_buffer[PARAM_LIST_POOL * 16]  __attribute__((section(".noinit"))); /* Estimated average size of buffers */
static uint32_t param_buffer_used = 0;

static param_heap_t * param_list_alloc(int type, int array_size) {

	int buffer_required = param_typesize(type) * array_size;
	while(buffer_required%4 != 0) buffer_required++; /* Ensure that all values are word-aliged */

	if (param_heap_used >= PARAM_LIST_POOL || param_buffer_used + buffer_required > sizeof(param_buffer)) {
		return NULL;
	}

	param_heap_t* param = &param_heap[param_heap_used];
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

typedef struct param_heap_s {
	param_t param;
	union {
		uint64_t alignme;
		uint8_t *buffer;
	};
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

void param_list_destroy(param_t * param) {
	param_list_destroy_impl(param);
}

param_t * param_list_create_remote(int id, int node, int type, uint32_t mask, int array_size, char * name, char * unit, char * help, int storage_type) {

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

	param->vmem = NULL;
	param->name = param_heap->name;
	param->addr = param_heap->buffer;
	param->timestamp = &param_heap->timestamp;
	param->unit = param_heap->unit;
	param->docstr = param_heap->help;

	param->id = id;
	param->node = node;
	param->type = type;
	param->mask = mask;
	param->array_size = array_size;
	param->array_step = param_typesize(type);

	strlcpy(param->name, name, 36);
	if (unit) {
		strlcpy(param->unit, unit, 10);
	}
	if (help) {
		strlcpy(param->docstr, help, 150);
	}

    param->alloc = 1;
    param->next = 0;

	return param;

}

#endif

