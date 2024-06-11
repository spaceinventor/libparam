/*
 * vmem_block.c
 *
 *  Created on: May 28, 2024
 *      Author: thomas lykkeberg
 */


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "vmem/vmem_block.h"

/* The symbols __start_vmem_bdevice and __stop_vmem_bdevice will only be generated if the user defines any VMEMs.
    We therefore use __attribute__((weak)) so we can compile in the absence of these. */
//int __start_vmem_bdevice __attribute__((weak)) = 0;
//int __stop_vmem_bdevice __attribute__((weak)) = 0;

extern int __start_vmem_bdevice, __stop_vmem_bdevice;

static uint8_t *cache_read(const vmem_block_driver_t *drv, uintptr_t address, uint32_t *length);
static uint32_t cache_write(const vmem_block_driver_t *drv, uintptr_t address, uintptr_t data, uint32_t length);
static void cache_flush(const vmem_block_driver_t *drv);
static bool address_in_cache(const vmem_block_driver_t *drv, uintptr_t address);

static bool address_in_cache(const vmem_block_driver_t *drv, uintptr_t address) {

    vmem_block_cache_t *cache = drv->device->cache;
    bool in_cache = false;
    
    if (cache->is_valid) {
        uint32_t cache_start = (cache->start_block * drv->device->bsize);
        if (address >= cache_start && address < (cache_start + cache->size)) {
            in_cache = true;
        }
    }

    return in_cache;

}

static void cache_flush(const vmem_block_driver_t *drv) {

    vmem_block_cache_t *cache = drv->device->cache;

    if (cache->is_modified && cache->is_valid) {
        int32_t res;
        printf("cache_flush()::the cache is modified, write it to device\n");
        res = drv->api.write(drv, cache->start_block, (cache->size / drv->device->bsize), cache->data);
        if (res) {
            printf("Error, could not write to block device '%s'\n", drv->device->name);
        } else {
            /* The cache is still valid, but now it is not modified anymore */
            cache->is_modified = false;
        }
    }

}

static uint32_t cache_write(const vmem_block_driver_t *drv, uintptr_t address, uintptr_t data, uint32_t length) {

    vmem_block_cache_t *cache = drv->device->cache;
    uint32_t size;

    printf("cache_write(%p,0x%"PRIXPTR",0x%"PRIXPTR"%"PRIu32")\n", drv, address, (uintptr_t)data, length);

    if (!address_in_cache(drv, address)) {
        printf("cache_write()::flush the cache and re-read from address %p\n", (void *)address);
        /* There is not a cache hit, so flush and read a new one */
        cache_flush(drv);
        /* Read in the cache and ignore the return value */
        (void)cache_read(drv, address, NULL);
    }

    /* The address is within the cache and it is valid */
    uint32_t block = (address / drv->device->bsize);
    uint32_t block_offset = (address % drv->device->bsize);
    uint32_t offset = (((block - cache->start_block) * drv->device->bsize) + block_offset);

    /* We need to adjust the amount we can actually write to the cache */
    if ((length + block_offset) > cache->size) {
        /* We are about to write more than the entire cache */
        size = cache->size - (length + block_offset);
        /* Copy the portion we can fit */
        memcpy(&cache->data[offset], (void *)data, size);
        cache->is_modified = true;
        /* Then flush it */
        cache_flush(drv);
    } else {
        size = length;
        memcpy(&cache->data[offset], (void *)data, size);
        cache->is_modified = true;
    }

    /* Signal the actual length written */
    return size;
}

static uint8_t *cache_read(const vmem_block_driver_t *drv, uintptr_t address, uint32_t *length) {

    vmem_block_cache_t *cache = drv->device->cache;
    uint32_t block = (address / drv->device->bsize);
    uint32_t block_offset = (address % drv->device->bsize);

    printf("cache_read(%p,0x%"PRIXPTR",%"PRIu32")\n", drv, address, *length);

    if (!address_in_cache(drv, address)) {
        printf("cache_read()::address not in cache, read from device\n");
        int32_t res;
        /* The current block is outside the cache or the cache is invalid */
        res = drv->api.read(drv, block, (cache->size / drv->device->bsize), cache->data);
        if (res) {
            printf("Error, could not read from block device '%s'\n", drv->device->name);
            return NULL;
        }
        cache->is_valid = true;
        cache->is_modified = false;
        cache->start_block = block;
    }

    if (length) {
        (*length) = (cache->size - block_offset);
    }

    /* Return the address of the data in the cache */
    return &cache->data[block_offset];

}

void vmem_block_read(vmem_t * vmem, uint32_t addr, void * dataout, uint32_t len) {

    vmem_block_region_t *reg = (vmem_block_region_t *)vmem->driver;
    uintptr_t physaddr = reg->physaddr + addr;
    uintptr_t destaddr = (uintptr_t)dataout;
    uintptr_t srcaddr = physaddr;

    printf("vmem_block_read(%"PRIXPTR",0x%"PRIX32",%"PRIXPTR",%"PRIu32")\n", (uintptr_t)vmem, addr, (uintptr_t)dataout, len);

    while (len) {
        uint8_t *data;
        uint32_t size;

        /* Read a chunk of data from the eMMC and cache it */
        data = cache_read(reg->driver, srcaddr, &size);
        /* Adjust the number of bytes to read from the cache */
        if (len < size) {
            size = len;
        }
        /* Now copy the data from the cache into the destination */
        memcpy((void *)destaddr, data, size);
        /* Update length, source and destination index' */
        len -= size;
        srcaddr += size;
        destaddr += size;
    }

}

void vmem_block_write(vmem_t * vmem, uint32_t addr, const void * datain, uint32_t len) {

    vmem_block_region_t *reg = (vmem_block_region_t *)vmem->driver;
    uintptr_t physaddr = reg->physaddr + addr;
    uintptr_t srcaddr = (uintptr_t)datain;
    uintptr_t destaddr = physaddr;

    printf("vmem_block_write(%"PRIXPTR",0x%"PRIX32",%"PRIXPTR",%"PRIu32")\n", (uintptr_t)vmem, addr, (uintptr_t)datain, len);

    while (len) {
        uint32_t nbytes;

        /* Try writing all we have, and adjust accordingly afterwards */
        nbytes = cache_write(reg->driver, destaddr, srcaddr, len);
        /* Update length, source and destination index' */
        len -= nbytes;
        srcaddr += nbytes;
        destaddr += nbytes;
    }

}

void vmem_block_init(void) {

    /* Calling these methods requires that the FreeRTOS scheduler is started, since it uses usleep() */
	for(vmem_block_device_t *dev = (vmem_block_device_t *)&__start_vmem_bdevice; dev < (vmem_block_device_t *)&__stop_vmem_bdevice; dev++) {
        /* Print some specifics for the particular block device */
        printf("Initializing VMEM block device: '%s'\n", dev->name);
        printf("   block size      : %"PRIu32" bytes\n", dev->bsize);
        printf("   number of blocks: %"PRIu32"\n", dev->total_nblocks);
        printf("   total size      : %"PRIu64" bytes\n", ((uint64_t)dev->bsize * (uint64_t)dev->total_nblocks));
        printf("   cache size      : %"PRIu32" bytes (%"PRIu32" blocks)\n", dev->cache->size, (dev->cache->size / dev->bsize));
        /* Then initialize it */
        (*dev->init)(dev);
    }

}

