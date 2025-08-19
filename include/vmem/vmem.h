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
} vmem_t __attribute__((aligned(__alignof__(struct vmem_s))));

/**
 * @brief Opaque data type to iterate over available VMEM objects
 * @sa
 */
struct vmem_iter_s;
typedef struct vmem_iter_s vmem_iter_t;


/**
 * @brief VMEM Memory copy function - 32-bit version ONLY
 *
 * This method is only capable of handling 32-bit source and destination
 * addresses, which means that it CAN NOT handle the newest 64-bit addressing
 *
 * @param to 32-bit destination address (virtual or physical)
 * @param from 32-bit source address (virtual or physical)
 * @param size Number fo bytes to copy
 * @return void*
 */
void * vmem_memcpy(void * to, const void * from, uint32_t size);

/**
 * @brief Write data from a CPU accessible memory area to a VMEM area
 * @param to valid VMEM virtual address the data will be written to
 * @param from pointer to source data
 * @param size size in bytes to write
 * @return NULL, in all cases, no error detection possible at this time
 */
void * vmem_write(uint64_t to, const void * from, uint32_t size);

/**
 * @brief Read data from a VMEM area to CPU accessible memory area
 * @param to pointer to valid memory of at least size bytes
 * @param from valid VMEM virtual address the data will be read from
 * @param size size in bytes to read
 * @return NULL, in all cases, no error detection possible at this time
 */
void * vmem_read(void * to, uint64_t from, uint32_t size);

/**
 * @brief Copy data between a VMEM area and CPU accessible memory area
 * 
 * @warning this function WILL NOT WORK when both the "to" and "from" parameter are VMEM virtual addresses!
 * 
 * @param to 64-bit integer representing either a valid VMEM virtual address or a CPU accessible memory area, will be used as destination
 * @param from 64-bit integer representing either a valid VMEM virtual address or a CPU accessible memory area, will be uses as source
 * @param size size in bytes to copy
 * @return NULL if the operation involved one VMEM, the result of call to memcpy() if no VMEM were given as "to" or "from" parameter
 */
void * vmem_cpy(uint64_t to, uint64_t from, uint32_t size);

/** 
 * @brief Get the VMEM object associated with the given index
 * 
 * VMEM objects are given an index from 0 to the number of VMEM objects present in a particular build
 * 
 * @param idx index to retrieve VMEM for
 * @return VMEM object for index if found, NULL otherwise
 * @sa vmem_ptr_to_index
 */
vmem_t * vmem_index_to_ptr(int idx);

/**
 * @brief Get index of given VMEM object
 * @param vmem pointer to VMEM object to get the index of
 * @return 0-based index of given VMEM object if found, -1 otherwise
 */
int vmem_ptr_to_index(vmem_t * vmem);

/**
 * @brief Return the VMEM object which includes the given vaddr in its address space
 * @param vaddr VMEM virtual address to obtain the VMEM object for
 * @return pointer to valid VMEM object of vaddr if found, NULL otherwise
 */
vmem_t * vmem_vaddr_to_vmem(uint64_t vaddr);

/**
 * @todo Document this
 * @brief 
 * @param vmem 
 * @return 
 */
int vmem_flush(vmem_t *vmem);

/**
 * Use this from an APM to register the VMEM objects it may define
 * @brief Register an array of VMEM objects
 * @param start pointer to the first element
 * @param stop pointer to the lasst element
 */
void vmem_add(vmem_t * start, vmem_t * stop);

/**
 * @brief Get the next VMEM list element from the given iterator
 * @param iter if NULL, will return a valid iterator from the first VMEM element
 * @return pointer to the next vmem_iter_t object, NULL if end of list is reached
 */
vmem_iter_t *vmem_next(vmem_iter_t * iter);

/**
 * @brief Get a pointer to the VMEM object of the given iterator
 * @param iter pointer to a valid iterator
 * @return pointer to valid vmem_t if iterator is valid, NULL otherwise
 * @sa vmem_next() to see how to obtain an iterator for the first VMEM object
 */
vmem_t *vmem_from_iter(vmem_iter_t * iter);

/**
 * @brief linker-generated symbol for the first VMEM element in the linker "vmem" section
 */
extern vmem_t __start_vmem __attribute__((weak));
/**
 * @brief linker-generated symbol for the last VMEM element in the linker "vmem" section
 */
extern vmem_t __stop_vmem __attribute__((weak));

#ifdef __cplusplus
}
#endif

#endif /* SRC_PARAM_VMEM_H_ */
