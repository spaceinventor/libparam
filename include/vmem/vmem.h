/*
 * vmem.h
 *
 *  Created on: Sep 22, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_VMEM_H_
#define SRC_PARAM_VMEM_H_

#ifdef __cplusplus
extern "C" {
#endif

#define VMEM_MAX(a,b) ((a) > (b) ? a : b)
#define VMEM_MIN(a,b) ((a) < (b) ? a : b)

#include <stddef.h>
#include <endian.h>
#include <param/param.h>

enum vmem_types{
	VMEM_TYPE_RAM = 1,
	VMEM_TYPE_FRAM = 2,
	VMEM_TYPE_FRAM_SECURE = 3,
	VMEM_TYPE_FLASH = 4,
	VMEM_TYPE_DRIVER = 5,
	VMEM_TYPE_FLASH_QSPI = 6,
	VMEM_TYPE_FILE = 7,
	VMEM_TYPE_FRAM_CACHE = 8,
	VMEM_TYPE_NOR_FLASH = 9,
	VMEM_TYPE_BLOCK = 10,
	VMEM_TYPE_UNKNOWN = -1,
};

typedef struct vmem_s {
	int type;
	void (*read)(struct vmem_s * vmem, uint64_t addr, void * dataout, uint32_t len);
	void (*write)(struct vmem_s * vmem, uint64_t addr, const void * datain, uint32_t len);
	int (*backup)(struct vmem_s * vmem);
	int (*restore)(struct vmem_s * vmem);
	int (*flush)(struct vmem_s * vmem);
	/* This anonymous union is needed to be able to handle 64-bit and 32-bit
	 * systems interchangeably. Since the VMEM backend always expects 64-bit
	 * vaddr, and we are not able to initialize the 64-bit vaddr field with
	 * a 32-bit address (the case for RAM VMEM's), we need to have a way of
	 * doing it with a little trick. */
	union {
		struct {

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
			uint32_t vaddr32;
			uint32_t vaddr_pad;
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
			uint32_t vaddr_pad;
			uint32_t vaddr32;
#else
#error "Cannot compile-time detect endianness"
#endif
		};
		uint64_t vaddr;
	};
	uint64_t size;
	const char *name;
	int big_endian;
	int ack_with_pull; // allow ack with pull request
	void * driver;
} vmem_t;

void * vmem_memcpy(void * to, const void * from, uint32_t size);
void * vmem_write(uint64_t to, const void * from, uint32_t size);
void * vmem_read(void * to, uint64_t from, uint32_t size);
void * vmem_cpy(uint64_t to, uint64_t from, uint32_t size);

vmem_t * vmem_index_to_ptr(int idx);
int vmem_ptr_to_index(vmem_t * vmem);
vmem_t * vmem_vaddr_to_vmem(uint64_t vaddr);
int vmem_flush(vmem_t *vmem);

extern int __start_vmem __attribute__((weak)), __stop_vmem __attribute__((weak));

#ifdef __cplusplus
}
#endif

#endif /* SRC_PARAM_VMEM_H_ */
