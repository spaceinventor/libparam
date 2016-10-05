/*
 * vmem.h
 *
 *  Created on: Sep 22, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_VMEM_H_
#define SRC_PARAM_VMEM_H_

#include <param.h>

typedef struct vmem_s {
	void (*read)(struct vmem_s * vmem, uint16_t addr, void * dataout, int len);
	void (*write)(struct vmem_s * vmem, uint16_t addr, void * datain, int len);
	void * vaddr;
	int size;
	const char *name;
	int big_endian;
	void * driver;
} vmem_t;

void vmem_dump(vmem_t * vmem);
void vmem_list(void);
void * vmem_memcpy(void * to, void * from, size_t size);
vmem_t * vmem_index_to_ptr(int idx);
int vmem_ptr_to_index(vmem_t * vmem);

#endif /* SRC_PARAM_VMEM_H_ */
