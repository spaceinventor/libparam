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
extern __attribute__((weak)) vmem_block_device_t __start_vmem_bdevice;
extern __attribute__((weak)) vmem_block_device_t __stop_vmem_bdevice;


static uint32_t cache_read(const vmem_block_driver_t *drv, vmem_block_cache_t *cache, uint64_t address, uintptr_t data, uint32_t length);
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

    uint32_t block_addr = (address / drv->device->bsize);
    uint32_t unalign = (address % drv->device->bsize);
    uint64_t addr = address;
    uint32_t size = 0;

    //printf("::cache_write(%p,0x%"PRIX64",0x%"PRIXPTR",%"PRIu32")\n", drv, address, (uintptr_t)data, length);

    if (!cache && data != (uintptr_t)NULL && length > 0) {
        int32_t res;
        uint32_t len = 0;

        if (unalign) {
            /* We need to read a least one block, if we are not aligned */
            res = drv->api.read(drv, block_addr, 1, drv->device->oneblock);
            if (res) { printf("Error, could not read from device\n"); return 0; }
            len = (drv->device->bsize - unalign);
            if (len > length) {
                len = length;
            }
            /* Update the part of the oneblock we actually need to write, and write it back */
            memcpy(&drv->device->oneblock[unalign], (void *)data, len);
            res = drv->api.write(drv, block_addr, 1, drv->device->oneblock);
            if (res) { printf("Error , could not write to device\n"); return 0; }
            data += len;
            addr += len;
        }
        
        /* Write the most part of what can be written directly as whole blocks */
        uint32_t nblocks = (length - len) / drv->device->bsize;
        if (nblocks > 0) {
            block_addr = addr / drv->device->bsize;
            res = drv->api.write(drv, block_addr, nblocks, (void *)data);
            if (res) { printf("Error, could not read from device\n"); return 0; }
            data += (nblocks * drv->device->bsize);
            addr += (nblocks * drv->device->bsize);
        }

        /* Finally we must write any remaining parts */
        if (addr < (address + length)) {
            block_addr = addr / drv->device->bsize;
            unalign = (address + length) % drv->device->bsize;
            if (unalign) {
                res = drv->api.read(drv, block_addr, 1, drv->device->oneblock);
                if (res) { printf("Error, could not read from device\n"); return 0; }
                /* Update the part of the oneblock we actually need to write, and write it back */
                memcpy(&drv->device->oneblock[0], (void *)data, unalign);
                res = drv->api.write(drv, block_addr, 1, drv->device->oneblock);
                if (res) { printf("Error , could not write to device\n"); return 0; }
                data += unalign;
            }
        }

        /* We wrote the entire length to the device */
        size = length;
    } else {
        /* Normal cache handling, since the length are shorter than the cache size */
        if (!address_in_cache(drv, cache, address)) {
            //printf("::cache_write() Flush the cache and re-read from address 0x%"PRIX64"\n", address);
            /* There is not a cache hit, so flush and read a new one */
            cache_flush(drv, cache);
            /* Read in the cache and ignore the return value */
            (void)cache_read(drv, cache, address, (uintptr_t)NULL, 0);
        }

        /* The address is within the cache and it is valid */
        uint32_t offset = (((block_addr - cache->start_block) * drv->device->bsize) + unalign);

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
    }

    /* Signal the actual length written */
    return size;
}

static uint32_t cache_read(const vmem_block_driver_t *drv, vmem_block_cache_t *cache, uint64_t address, uintptr_t data, uint32_t length) {

    uint32_t block_addr = (address / drv->device->bsize);
    uint32_t unalign = (address % drv->device->bsize);
    uint32_t size = 0;
    uint64_t addr = address;

    //printf("::cache_read(%p,0x%"PRIX64",0x%"PRIXPTR",%"PRIu32")\n", drv, address, data, length);

    if (!cache && data != (uintptr_t)NULL && length > 0) {
        int32_t res;
        uint32_t len = 0;

        /* If the starting address is not block aligned, we need to handle that special */
        if (unalign) {
            res = drv->api.read(drv, block_addr, 1, drv->device->oneblock);
            if (res) { printf("Error, could no read from device '%s'\n", drv->device->name); }
            /* Calculate the amount to move */
            len = (drv->device->bsize - unalign);
            if (len > length) {
                len = length;
            }
            /* Move the data from oneblock into the receiving buffer */
            memcpy((void *)data, &drv->device->oneblock[unalign], len);
            data += len;
            addr += len;
        }

        /* Read the most of what we can of whole blocks */
        uint32_t nblocks = (length - len) / drv->device->bsize;
        if (nblocks > 0) {
            block_addr = addr / drv->device->bsize;
            res = drv->api.read(drv, block_addr, nblocks, (void *)data);
            if (res) { printf("Error, could no read from device '%s'\n", drv->device->name); }
            data += (nblocks * drv->device->bsize);
            addr += (nblocks * drv->device->bsize);
        }

        /* Finally we must possibly read an unaligned block at the end */
        if (addr < (address + length)) {
            block_addr = addr / drv->device->bsize;
            unalign = (address + length) % drv->device->bsize;
            if (unalign) {
                res = drv->api.read(drv, block_addr, 1, drv->device->oneblock);
                if (res) { printf("Error, could no read from device '%s'\n", drv->device->name); }
                /* Update the receiving buffer with the part from the oneblock we actually have to read */
                memcpy((void *)data, &drv->device->oneblock[0], unalign);
                data += unalign;
            }
        }

        /* We read the entire length from the device */
        size = length;
    } else {
        /* Normal cache handling, since the length are shorter than the cache size */
        if (!address_in_cache(drv, cache, address)) {
            //printf("::cache_read() Address not in cache, read from device\n");
            int32_t res;
            /* The current block is outside the cache or the cache is invalid */
            res = drv->api.read(drv, block_addr, (cache->size / drv->device->bsize), cache->data);
            if (res) {
                printf("Error, could not read from block device '%s'\n", drv->device->name);
                return 0;
            }
            cache->is_valid = true;
            cache->is_modified = false;
            cache->start_block = block_addr;
        }

        uint32_t offset = (((block_addr - cache->start_block) * drv->device->bsize) + unalign);

        size = (cache->size - offset);
        if (size > length) {
            size = length;
        }

        if (data != 0) {
            /* Now copy the data from the cache into the destination */
            memcpy((void *)data, &cache->data[offset], size);
        }
    }

    /* Return the size of the data just read from the cache */
    return size;
}

void vmem_block_read(vmem_t * vmem, uint64_t addr, void * dataout, uint32_t len) {

    vmem_block_region_t *reg = (vmem_block_region_t *)vmem->driver;
    uint64_t physaddr = reg->physaddr + addr;
    uintptr_t destaddr = (uintptr_t)dataout;
    uint64_t srcaddr = physaddr;

    //printf("vmem_block_read(%"PRIXPTR",0x%"PRIX64",%"PRIXPTR",%"PRIu32") => 0x%"PRIX64"\n", (uintptr_t)vmem, addr, (uintptr_t)dataout, len, srcaddr);

    while (len) {
        uint32_t nbytes = 0;

        /* Read a chunk of data from the eMMC and cache it */
        nbytes = cache_read(reg->driver, reg->cache, srcaddr, destaddr, len);
        /* Adjust the number of bytes to read from the cache */
        if (len < nbytes) {
            nbytes = len;
        }
        /* Update length, source and destination index' */
        len -= nbytes;
        srcaddr += nbytes;
        destaddr += nbytes;
    }

}

void vmem_block_write(vmem_t * vmem, uint64_t addr, const void * datain, uint32_t len) {

    vmem_block_region_t *reg = (vmem_block_region_t *)vmem->driver;
    uint64_t physaddr = reg->physaddr + addr;
    uintptr_t srcaddr = (uintptr_t)datain;
    uint64_t destaddr = physaddr;

    //printf("vmem_block_write(%"PRIXPTR",0x%"PRIX64",%"PRIXPTR",%"PRIu32") => 0x%"PRIX64"\n", (uintptr_t)vmem, addr, (uintptr_t)datain, len, destaddr);

    /* Split it up into parts decided by the caching layer */
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
    //printf("vmem_block_flush(%p)\n", vmem);

    if (vmem->type == VMEM_TYPE_BLOCK) {
        vmem_block_region_t *region = (vmem_block_region_t *)vmem->driver;
        if (region->cache) {
            /* If the region has a cache object attached, flush it */
            cache_flush(region->driver, region->cache);
        }
        res = 0;
    }

    return res;
}

void vmem_block_init(void) {

    /* Calling these methods requires that the FreeRTOS scheduler is started, since it uses usleep() */
    if ((&__start_vmem_bdevice) && (&__stop_vmem_bdevice)) {
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

