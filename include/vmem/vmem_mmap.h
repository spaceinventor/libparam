#pragma once

#include <vmem/vmem.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	void * physaddr;
	char * filename;
} vmem_mmap_driver_t;

void vmem_mmap_read(vmem_t * vmem, uint64_t addr, void * dataout, uint32_t len);
void vmem_mmap_write(vmem_t * vmem, uint64_t addr, const void * datain, uint32_t len);

#define VMEM_DEFINE_MMAP(name_in, strname, filename_in, size_in) \
	static vmem_mmap_driver_t vmem_mmap_##name_in##_driver = { \
		.physaddr = 0, \
		.filename = filename_in, \
	}; \
	__attribute__((section("vmem"))) \
	__attribute__((aligned(1))) \
	__attribute__((used)) \
	vmem_t vmem_mmap_##name_in = { \
		.type = VMEM_TYPE_FILE, \
		.name = strname, \
		.size = size_in, \
		.read = vmem_mmap_read, \
		.write = vmem_mmap_write, \
		.driver = &vmem_mmap_##name_in##_driver, \
		.vaddr = 0, \
		.ack_with_pull = 1, \
	};

/// Helper macro to help reference the vmem_t variable created behind the scene by the VMEM_DEFINE_MMAP macro above
#define VMEM_MMAP_VAR(name_in) vmem_mmap_##name_in

#ifdef __cplusplus
}
#endif
