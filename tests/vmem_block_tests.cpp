#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdio.h>
#include "vmem/vmem.h"
#include "vmem/vmem_block.h"

using namespace std;

#define EMMC_BLOCK_SIZE     512
#define EMMC_CACHED_BLOCKS  2
#define STFW_FIFO_SIZE      100*1024

extern "C" int32_t binit_emmc(const vmem_block_device_t *dev);
extern "C" int32_t bread_emmc(const vmem_block_driver_t *drv, uint32_t blockaddr, uint32_t n_blocks, uint8_t *data);
extern "C" int32_t bwrite_emmc(const vmem_block_driver_t *drv, uint32_t blockaddr, uint32_t n_blocks, uint8_t *data);

VMEM_DEFINE_BLOCK_DEVICE(emmc0, "emmc0", EMMC_BLOCK_SIZE, 16777216, binit_emmc);
VMEM_DEFINE_BLOCK_DRIVER(emmc, "emmc", bread_emmc, bwrite_emmc, emmc0);

VMEM_DEFINE_BLOCK_CACHE(emmc_cache_reg0, EMMC_BLOCK_SIZE * 100);
VMEM_DEFINE_BLOCK_CACHE(emmc_cache_reg1, EMMC_BLOCK_SIZE * 50);
VMEM_DEFINE_BLOCK_CACHE(emmc_cache_reg2, EMMC_BLOCK_SIZE * 10);
VMEM_DEFINE_BLOCK_CACHE(emmc_cache_reg3, EMMC_BLOCK_SIZE * 2);
VMEM_DEFINE_BLOCK_CACHE(emmc_cache_reg4, EMMC_BLOCK_SIZE * 1);

VMEM_DEFINE_BLOCK_REGION(stfw0, "stfw0", 0x0 + (0 * STFW_FIFO_SIZE), STFW_FIFO_SIZE, 0x1000000000ULL + (0 * STFW_FIFO_SIZE), emmc, &vmem_emmc_cache_reg0_cache);
VMEM_DEFINE_BLOCK_REGION(stfw1, "stfw1", 0x0 + (1 * STFW_FIFO_SIZE), STFW_FIFO_SIZE, 0x1000000000ULL + (1 * STFW_FIFO_SIZE), emmc, &vmem_emmc_cache_reg1_cache);
VMEM_DEFINE_BLOCK_REGION(stfw2, "stfw2", 0x0 + (2 * STFW_FIFO_SIZE), STFW_FIFO_SIZE, 0x1000000000ULL + (2 * STFW_FIFO_SIZE), emmc, &vmem_emmc_cache_reg2_cache);
VMEM_DEFINE_BLOCK_REGION(stfw3, "stfw3", 0x0 + (3 * STFW_FIFO_SIZE), STFW_FIFO_SIZE, 0x1000000000ULL + (3 * STFW_FIFO_SIZE), emmc, &vmem_emmc_cache_reg3_cache);
VMEM_DEFINE_BLOCK_REGION(stfw4, "stfw4", 0x0 + (4 * STFW_FIFO_SIZE), STFW_FIFO_SIZE, 0x1000000000ULL + (4 * STFW_FIFO_SIZE), emmc, &vmem_emmc_cache_reg4_cache);

#define TEST_BUFFER_SIZE 10240
#define TEST_BUFFER_OFFSET 78
#define TEST_READ_CHUNK_SIZE 512
#define TEST_WRITE_CHUNK_SIZE 192

#define EMMC_SIZE 5*100*1024
uint8_t g_emmc_data_read[EMMC_SIZE];
uint8_t g_emmc_data_write[EMMC_SIZE];
uint8_t *g_emmc_data_ptr;

extern "C" int32_t binit_emmc(const vmem_block_device_t *dev) {

    return 0;
}

extern "C" int32_t bread_emmc(const vmem_block_driver_t *drv, uint32_t blockaddr, uint32_t n_blocks, uint8_t *data) {

    int32_t res = 0;
    memcpy(data, &g_emmc_data_ptr[(blockaddr * drv->device->bsize)], (n_blocks * drv->device->bsize));
    return res;
}

extern "C" int32_t bwrite_emmc(const vmem_block_driver_t *drv, uint32_t blockaddr, uint32_t n_blocks, uint8_t *data) {

    int32_t res = 0;
    memcpy(&g_emmc_data_ptr[(blockaddr * drv->device->bsize)], data, (n_blocks * drv->device->bsize));
    return res;
}

void generate_series_data(uint8_t *data, uint32_t length) {

    for (uint32_t n = 0; n < length; n++) {
        data[n] = (n % 255) + 1;
    }
}

void generate_random_data(uint8_t *data, uint32_t length) {

    // Seed the random number generator with the current time
    srand((unsigned)time(0));
    for (uint32_t n = 0; n < length; n++) {
        data[n] = (rand()%255 + 1);
    }
}

TEST(vmem_block, upload_random_download_compare) {

    uint8_t write_buffer[TEST_BUFFER_SIZE];
    uint8_t read_buffer[TEST_BUFFER_SIZE];
    uint32_t offset;

    // 0. Initialize the VMEM block module
    vmem_block_init();

    // 1. Generate a random data set of TEST_BUFFER_SIZE (0 is omitted)
    generate_random_data(&write_buffer[0], TEST_BUFFER_SIZE);

    // 2. Generate an empty "eMMC"
    g_emmc_data_ptr = &g_emmc_data_write[0];
    memset(g_emmc_data_ptr, 0, EMMC_SIZE);

    // 3. Write the random test buffer to the empty "eMMC"
    for (offset=0;offset<TEST_BUFFER_SIZE;offset+=TEST_WRITE_CHUNK_SIZE) {
        uint32_t size;
        void *addr;
        addr = (void *)(((uintptr_t)vmem_stfw3.vaddr + offset));
        if (TEST_BUFFER_SIZE - offset < TEST_WRITE_CHUNK_SIZE) {
            size = TEST_BUFFER_SIZE - offset;
        } else {
            size = TEST_WRITE_CHUNK_SIZE;
        }
        vmem_memcpy(addr, &write_buffer[offset], size);
    }

    // 4. Flush any cached data to the "eMMC"
    vmem_block_flush(&vmem_stfw3);

    // 5. Verify that the data was written correctly to the "eMMC"
    EXPECT_TRUE( 0 == memcmp(&write_buffer[0], &g_emmc_data_ptr[STFW_FIFO_SIZE * 3], TEST_BUFFER_SIZE) );

    // 6. Clear out the readback buffer
    memset(&read_buffer[0], 0 , TEST_BUFFER_SIZE);

    // 6. Read the test buffer from the "eMMC" in chunks
    for (offset=0;offset<TEST_BUFFER_SIZE;offset+=TEST_READ_CHUNK_SIZE) {
        uint32_t size;
        void *addr;
        addr = (void *)(((uintptr_t)vmem_stfw3.vaddr + offset));
        if (TEST_BUFFER_SIZE - offset < TEST_READ_CHUNK_SIZE) {
            size = TEST_BUFFER_SIZE - offset;
        } else {
            size = TEST_READ_CHUNK_SIZE;
        }
        vmem_memcpy(&read_buffer[offset], addr, size);
    }

    // 7. Compare the content expected to be in the writeable "eMMC"
    EXPECT_TRUE( 0 == memcmp(&read_buffer[0], &write_buffer[0], TEST_BUFFER_SIZE) );

}

TEST(vmem_block, upload_random_download_compare_at_offset) {

    uint8_t write_buffer[TEST_BUFFER_SIZE];
    uint8_t read_buffer[TEST_BUFFER_SIZE];
    uint32_t offset;

    // 0. Initialize the VMEM block module
    vmem_block_init();

    // 1. Generate a random data set of TEST_BUFFER_SIZE (0 is omitted)
    generate_random_data(&write_buffer[0], TEST_BUFFER_SIZE);

    // 2. Generate an empty "eMMC"
    g_emmc_data_ptr = &g_emmc_data_write[0];
    memset(g_emmc_data_ptr, 0, EMMC_SIZE);

    // 3. Write the random test buffer to the empty "eMMC"
    for (offset=0;offset<TEST_BUFFER_SIZE;offset+=TEST_WRITE_CHUNK_SIZE) {
        uint32_t size;
        void *addr;
        addr = (void *)(((uintptr_t)vmem_stfw3.vaddr + TEST_BUFFER_OFFSET + offset));
        if (TEST_BUFFER_SIZE - offset < TEST_WRITE_CHUNK_SIZE) {
            size = TEST_BUFFER_SIZE - offset;
        } else {
            size = TEST_WRITE_CHUNK_SIZE;
        }
        vmem_memcpy(addr, &write_buffer[offset], size);
    }

    // 4. Flush any cached data to the "eMMC"
    vmem_block_flush(&vmem_stfw3);

    // 5. Verify that the data was written correctly to the "eMMC"
    EXPECT_TRUE( 0 == memcmp(&write_buffer[0], &g_emmc_data_ptr[STFW_FIFO_SIZE * 3 + TEST_BUFFER_OFFSET], TEST_BUFFER_SIZE) );

    // 6. Clear out the readback buffer
    memset(&read_buffer[0], 0 , TEST_BUFFER_SIZE);

    // 6. Read the test buffer from the "eMMC" in chunks
    for (offset=0;offset<TEST_BUFFER_SIZE;offset+=TEST_READ_CHUNK_SIZE) {
        uint32_t size;
        void *addr;
        addr = (void *)(((uintptr_t)vmem_stfw3.vaddr + TEST_BUFFER_OFFSET + offset));
        if (TEST_BUFFER_SIZE - offset < TEST_READ_CHUNK_SIZE) {
            size = TEST_BUFFER_SIZE - offset;
        } else {
            size = TEST_READ_CHUNK_SIZE;
        }
        vmem_memcpy(&read_buffer[offset], addr, size);
    }

    // 7. Compare the content expected to be in the writeable "eMMC"
    EXPECT_TRUE( 0 == memcmp(&read_buffer[0], &write_buffer[0], TEST_BUFFER_SIZE) );

}