/*
 * vmem.h
 *
 *  Created on: Sep 22, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_VMEM_H_
#define SRC_PARAM_VMEM_H_

#include <stddef.h>
#include <param/param.h>

typedef const struct vmem_s {
	void (*read)(const struct vmem_s * vmem, uint16_t addr, void * dataout, int len);
	void (*write)(const struct vmem_s * vmem, uint16_t addr, void * datain, int len);
	void * vaddr;
	int size;
	const char *name;
	int big_endian;
	const void * driver;
} vmem_t;

void vmem_dump(vmem_t * vmem);
void vmem_list(void);
void * vmem_memcpy(void * to, void * from, size_t size);
vmem_t * vmem_index_to_ptr(int idx);
int vmem_ptr_to_index(vmem_t * vmem);

#endif /* SRC_PARAM_VMEM_H_ */
