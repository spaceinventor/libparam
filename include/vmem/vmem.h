/*
 * vmem.h
 *
 *  Created on: Sep 22, 2016
 *      Author: johan
 */

#ifndef SRC_PARAM_VMEM_H_
#define SRC_PARAM_VMEM_H_

#define VMEM_MAX(a,b) ((a) > (b) ? a : b)
#define VMEM_MIN(a,b) ((a) < (b) ? a : b)

#include <stddef.h>
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
};

typedef struct vmem_s vmem_t;
struct vmem_s {
	int type;
	void (*read)(struct vmem_s * vmem, uint32_t addr, void * dataout, int len);
	void (*write)(struct vmem_s * vmem, uint32_t addr, void * datain, int len);
	int (*backup)(struct vmem_s * vmem);
	int (*restore)(struct vmem_s * vmem);
	void * vaddr;
	int size;
	const char *name;
	int big_endian;
	void * driver;
    vmem_t * next;
};

void * vmem_memcpy(void * to, void * from, size_t size);
vmem_t * vmem_index_to_ptr(int idx);
int vmem_ptr_to_index(vmem_t * vmem);


/**
 * Addin support
 * Definition of separate ELF section and references to the start and stop symbols as
 * are added by the compiler. 
 * The get_param_pointers is defined for an addin to enable access to the section by the addin loader. 
 */

#define VMEM_SECTION_INIT_NO_FUNC(secname)\
    extern int __start_ ## secname;\
    extern int __stop_ ## secname;\
    __attribute__((unused)) static vmem_t * vmem_section_start = (vmem_t*)&__start_ ## secname;\
    __attribute__((unused)) static vmem_t * vmem_section_stop = (vmem_t*)&__stop_ ## secname;

#define VMEM_SECTION_INIT_FUNC\
	__attribute__((unused)) void get_vmem_pointers(vmem_t ** start, vmem_t ** stop) {\
		*start = vmem_section_start;\
		*stop = vmem_section_stop;\
	}

#define VMEM_SECTION_INIT(secname)\
	VMEM_SECTION_INIT_NO_FUNC(secname)\
	VMEM_SECTION_INIT_FUNC

vmem_t * vmem_list_add_section(vmem_t * head, vmem_t * start, vmem_t *stop);
vmem_t * vmem_list_head();
vmem_t * vmem_list_iterate(vmem_t * vmem);

#endif /* SRC_PARAM_VMEM_H_ */
