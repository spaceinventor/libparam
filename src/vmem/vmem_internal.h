#pragma once

#include <vmem/vmem.h>

/**
 * @brief data type to iterate over available VMEM objects
 */
struct vmem_iter_s {
	vmem_t *start;
	vmem_t *stop;
	vmem_t *current;
	int idx;
	struct vmem_iter_s * next;
};
typedef struct vmem_iter_s vmem_iter_t;

/**
 * @brief Get the next VMEM list element from the given iterator
 * @param iter valid iterator, set "current" field to NULL to start iteration at the beginning
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
 * @brief Write chunk of data to VMEM from physical memory
 *
 * @param to Virtual address to write the data to
 * @param from Physical address to read the data from
 * @param size Number of bytes to transfer
 * @return void* always NULL, no error detection possible at this time
 */
void * vmem_write_direct(const vmem_t * vmem, uint64_t to, const void * from, uint32_t size);

/**
 * @brief Read chunk of data from VMEM to physical memory
 *
 * @param to Physical address to write the data to
 * @param from Virtual address to read the data from
 * @param size Number of bytes to transfer
 * @return void* always NULL, no error detection possible at this time
 */
void * vmem_read_direct(const vmem_t * vmem, void * to, uint64_t from, uint32_t size);
