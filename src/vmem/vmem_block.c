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
#include <inttypes.h>

#include "vmem/vmem_block.h"

/* The symbols __start_vmem_bdevice and __stop_vmem_bdevice will only be generated if the user defines any VMEMs.
    We therefore use __attribute__((weak)) so we can compile in the absence of these. */
__attribute__((weak)) int __start_vmem_bdevice = 0;
__attribute__((weak)) int __stop_vmem_bdevice = 0;

static uint8_t *cache_read(const vmem_block_driver_t *drv, vmem_block_cache_t *cache, uint64_t address, uint32_t *length);
static uint32_t cache_write(const vmem_block_driver_t *drv, vmem_block_cache_t *cache, uint64_t address, uintptr_t data, uint32_t length);
static void cache_flush(const vmem_block_driver_t *drv, vmem_block_cache_t *cache);
static bool address_in_cache(const vmem_block_driver_t *drv, vmem_block_cache_t *cache, uint64_t address);

static bool address_in_cache(const vmem_block_driver_t *drv, vmem_block_cache_t *cache, uint64_t address) {

    bool in_cache = false;
    
    if (cache->is_valid) {
        uint64_t cache_start = ((uint64_t)cache->start_block * (uint64_t)drv->device->bsize);
        if (address >= cache_start && address < (cache_start + cache->size)) {
            in_cache = true;
        }
    }

    return in_cache;

}

static void cache_flush(const vmem_block_driver_t *drv, vmem_block_cache_t *cache) {

    if (cache->is_modified && cache->is_valid) {
        int32_t res;
        //printf("::cache_flush() The cache is modified, write it to device\n");
        res = drv->api.write(drv, cache->start_block, (cache->size / drv->device->bsize), cache->data);
        if (res) {
            printf("Error, could not write to block device '%s'\n", drv->device->name);
        } else {
            /* The cache is still valid, but now it is not modified anymore */
            cache->is_modified = false;
        }
    }

}

static uint32_t cache_write(const vmem_block_driver_t *drv, vmem_block_cache_t *cache, uint64_t address, uintptr_t data, uint32_t length) {

    uint32_t size;

    //printf("::cache_write(%p,0x%"PRIX64",0x%"PRIXPTR",%"PRIu32")\n", drv, address, (uintptr_t)data, length);

    if (!address_in_cache(drv, cache, address)) {
        printf("::cache_write() Flush the cache and re-read from address 0x%"PRIX64"\n", address);
        /* There is not a cache hit, so flush and read a new one */
        cache_flush(drv, cache);
        /* Read in the cache and ignore the return value */
        (void)cache_read(drv, cache, address, NULL);
    }

    /* The address is within the cache and it is valid */
    uint32_t block = (address / drv->device->bsize);
    uint32_t block_offset = (address % drv->device->bsize);
    uint32_t offset = (((block - cache->start_block) * drv->device->bsize) + block_offset);

    /* We need to adjust the amount we can actually write to the cache */
    if ((length + offset) > cache->size) {
        /* We are about to write more than the entire cache */
        size = cache->size - offset;
        /* Copy the portion we can fit */
        memcpy(&cache->data[offset], (void *)data, size);
        cache->is_modified = true;
        /* Then flush it */
        cache_flush(drv, cache);
    } else {
        size = length;
        memcpy(&cache->data[offset], (void *)data, size);
        cache->is_modified = true;
    }

    /* Signal the actual length written */
    return size;
}

static uint8_t *cache_read(const vmem_block_driver_t *drv, vmem_block_cache_t *cache, uint64_t address, uint32_t *length) {

    uint32_t block = (address / drv->device->bsize);
    uint32_t block_offset = (address % drv->device->bsize);

    //printf("::cache_read(%p,0x%"PRIX64",%"PRIu32")\n", drv, address, (length ? (*length) : 0));

    if (!address_in_cache(drv, cache, address)) {
        //printf("::cache_read() Address not in cache, read from device\n");
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

    uint32_t offset = (((block - cache->start_block) * drv->device->bsize) + block_offset);

    if (length) {
        (*length) = (cache->size - offset);
    }

    /* Return the address of the data in the cache */
    return &cache->data[offset];

}

void vmem_block_read(vmem_t * vmem, uint64_t addr, void * dataout, uint32_t len) {

    vmem_block_region_t *reg = (vmem_block_region_t *)vmem->driver;
    uint64_t physaddr = reg->physaddr + addr;
    uintptr_t destaddr = (uintptr_t)dataout;
    uint64_t srcaddr = physaddr;

    //printf("vmem_block_read(%"PRIXPTR",0x%"PRIX64",%"PRIXPTR",%"PRIu32") => 0x%"PRIX64"\n", (uintptr_t)vmem, addr, (uintptr_t)dataout, len, srcaddr);

    while (len) {
        uint8_t *data;
        uint32_t size = 0;

        /* Read a chunk of data from the eMMC and cache it */
        data = cache_read(reg->driver, reg->cache, srcaddr, &size);
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

void vmem_block_write(vmem_t * vmem, uint64_t addr, const void * datain, uint32_t len) {

    vmem_block_region_t *reg = (vmem_block_region_t *)vmem->driver;
    uint64_t physaddr = reg->physaddr + addr;
    uintptr_t srcaddr = (uintptr_t)datain;
    uint64_t destaddr = physaddr;

    //printf("vmem_block_write(%"PRIXPTR",0x%"PRIX64",%"PRIXPTR",%"PRIu32") => 0x%"PRIX64"\n", (uintptr_t)vmem, addr, (uintptr_t)datain, len, destaddr);

    while (len) {
        uint32_t nbytes;

        /* Try writing all we have, and adjust accordingly afterwards */
        nbytes = cache_write(reg->driver, reg->cache, destaddr, srcaddr, len);
        /* Update length, source and destination index' */
        len -= nbytes;
        srcaddr += nbytes;
        destaddr += nbytes;
    }

}

int vmem_block_flush(vmem_t * vmem) {

    int res = 1;
    printf("vmem_block_flush(%p)\n", vmem);

    if (vmem->type == VMEM_TYPE_BLOCK) {
        vmem_block_region_t *region = (vmem_block_region_t *)vmem->driver;
        cache_flush(region->driver, region->cache);
        res = 0;
    }

    return res;
}

void vmem_block_init(void) {

    /* Calling these methods requires that the FreeRTOS scheduler is started, since it uses usleep() */
    if (__start_vmem_bdevice && __stop_vmem_bdevice) {
        for(vmem_block_device_t *dev = (vmem_block_device_t *)&__start_vmem_bdevice; dev < (vmem_block_device_t *)&__stop_vmem_bdevice; dev++) {
            /* Print some specifics for the particular block device */
            printf("Initializing VMEM block device: '%s'\n", dev->name);
            printf("   block size      : %"PRIu32" bytes\n", dev->bsize);
            printf("   number of blocks: %"PRIu32"\n", dev->total_nblocks);
            printf("   total size      : %"PRIu64" bytes\n", ((uint64_t)dev->bsize * (uint64_t)dev->total_nblocks));
            /* Then initialize it */
            if (dev->init) {
                (*dev->init)(dev);
            }
        }
    }

}

