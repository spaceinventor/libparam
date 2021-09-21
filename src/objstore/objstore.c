

#include <objstore/objstore.h>

#include <stdio.h>
#include <stdlib.h>


//VMEM_DEFINE_FILE(schedule, "schedule", "schedule.cnf", 0x1000);

const uint8_t sync_word[4] = {0xCA, 0x70, 0xCA, 0xFE};

//typedef int (*objstore_scan_callback_f)(vmem_t * vmem, int offset, int verbose);

static uint8_t _make_checksum(vmem_t * vmem, int offset, int length) {
    if (length < 0)
        return 0;

    uint8_t buf, checksum;
    vmem->read(vmem, offset+7, &checksum, sizeof(checksum)); // initialize with first data byte
    for (int i = 1; i < length; i++) {
        vmem->read(vmem, offset+7+i, &buf, sizeof(buf));
        checksum ^= buf;
    }

    return checksum;
}

static int _valid_checksum(void * data, uint16_t length, uint8_t checksum) {
    if (length < 0)
        return 0;
    
    uint8_t temp_checksum = *(uint8_t *) data;
    for (int i = 1; i < length; i++) {
        uint8_t * ptr = (uint8_t *) ( (long int) data + i);
        temp_checksum ^= *ptr;
    }

    if (checksum == temp_checksum) {
        return 1;
    } else {
        return 0;
    }
}

static int _valid_obj_check(vmem_t * vmem, int offset) {
    uint8_t data;
    vmem->read(vmem, offset, &data, sizeof(data));
    int sync_status = 0;
    if (data == sync_word[0]) {
        sync_status = 1;
        for (int j = 1; j < 4; j++) {
            vmem->read(vmem, offset+j, &data, sizeof(data));
            if (data == sync_word[sync_status]) {
                sync_status++;
            } else {
                return 0;
            }
        }
    } else {
        return 0;
    }

    return 1;
}

static int _valid_obj_data_check(vmem_t * vmem, int offset, void * data, uint16_t length) {
    uint8_t checksum;
    vmem->read(vmem, offset+7+length, &checksum, sizeof(checksum));

    if (_valid_checksum(data, length, checksum)) {
        return 1;
    } else {
        return 0;
    }
}

int objstore_alloc(vmem_t * vmem, int length, int verbose) {
    uint8_t sync_status = 0;
    int counter = 0;

    for (int i = 0; i < vmem->size; i++) {
        uint8_t data;
        vmem->read(vmem, i, &data, sizeof(data));

        if (data == sync_word[0]) {
            sync_status = 1;
            for (int j = 1; j < 4; j++) {

                vmem->read(vmem, i+j, &data, sizeof(data));
                if (data == sync_word[sync_status]) {
                    sync_status++;
                } else {
                    sync_status = 0;
                    counter++;
                    break;
                }

                if (sync_status == 4) {
                    sync_status = 0;
                    counter = 0;

                    uint16_t length;
                    // read length to skip ahead
                    vmem->read(vmem, i+5, &length, sizeof(length));
                    i += data+6;
                }
            }
        } else {
            sync_status = 0;
            counter++;
            if ( counter == (length+8) ) {
                if (verbose)
                    printf("Found room for %u bytes plus overhead starting at offset %u\n", length, i-(length+6));
                
                // todo: Lock the allocated piece of memory
                return i-(length+7);
            }
        }
    }

    return -1;

}

int objstore_read_obj_length(vmem_t * vmem, int offset) {
    if (_valid_obj_check(vmem, offset) == 0) {
        return -1;
    }
    uint16_t length;
    vmem->read(vmem, offset+5, &length, sizeof(length));

    return length;
}

int objstore_read_obj_type(vmem_t * vmem, int offset) {
    if (_valid_obj_check(vmem, offset) == 0) {
        return -1;
    }
    uint8_t type;
    vmem->read(vmem, offset+4, &type, sizeof(type));

    return type;
}

int objstore_scan(vmem_t * vmem, objstore_scan_callback_f callback, int verbose, void * ctx) {
    uint8_t sync_status = 0;
    
    for (int i = 0; i < vmem->size; i++) {
        uint8_t data;
        vmem->read(vmem, i, &data, sizeof(data));

        if (data == sync_word[0]) {
            sync_status = 1;
            for (int j = 1; j < 4; j++) {
                vmem->read(vmem, i+j, &data, sizeof(data));
                if (data == sync_word[sync_status]) {
                    sync_status++;
                } else {
                    sync_status = 0;
                    break;
                }

                if (sync_status == 4) {

                    if (callback(vmem, i, verbose, ctx) < 0)
                        return i;
                    
                    sync_status = 0;

                    uint16_t length;
                    vmem->read(vmem, i+5, &length, sizeof(length));
                    i += length+6;
                }
            }
        } else {
            sync_status = 0;
        }
    }

    return -1;
}

int objstore_rm_obj(vmem_t * vmem, int offset, int verbose) {
    // check that a valid object is actually present
    if (_valid_obj_check(vmem, offset) == 0) {
        if (verbose)
            printf("Obj deletion error! No valid object found at offset %u\n", offset);
        
        return -1;
    }

    const uint16_t length;
    vmem->read(vmem, offset+5, &length, sizeof(length));

    const uint8_t clear_block = 0xFF;
    // clear checksum
    vmem->write(vmem, offset+length+7, &clear_block, sizeof(clear_block));
    // clear data field
    for (int i = length; i > 0; i--) {
        vmem->write(vmem, offset+i+6, &clear_block, sizeof(clear_block));
    }
    // clear header
    vmem->write(vmem, offset+6, &clear_block, sizeof(clear_block));
    vmem->write(vmem, offset+5, &clear_block, sizeof(clear_block));
    vmem->write(vmem, offset+4, &clear_block, sizeof(clear_block));
    // clear sync word
    for (int i = 0; i < 4; i++) {
        vmem->write(vmem, offset+i, &clear_block, sizeof(clear_block));
    }

    if (verbose)
        printf("Deleted object of length %u bytes at offset %u\n", length, offset);
    
    return 0;
}

int objstore_read_obj(vmem_t * vmem, int offset, void * data_buf, int verbose) {
    if (_valid_obj_check(vmem, offset) == 0) {
        if (verbose)
            printf("Obj read error! No valid object found at offset %u\n", offset);
        
        return -1;
    }

    /* Data buffer must be correct size, e.g. by using objstore_read_obj_length */
    const uint16_t length;
    vmem->read(vmem, offset+5, &length, sizeof(length));

    vmem->read(vmem, offset+7, data_buf, length);

    if (_valid_obj_data_check(vmem, offset, data_buf, length) == 0)
        return -1;

    return 0;
}

void objstore_write_obj(vmem_t * vmem, int offset, uint8_t type, uint16_t length, void * data) {
    // TODO: check for lock
    vmem->write(vmem, offset, sync_word, 4);
    vmem->write(vmem, offset+4, &type, sizeof(type));
    vmem->write(vmem, offset+5, &length, sizeof(length));
    vmem->write(vmem, offset+7, data, length);

    uint8_t checksum = _make_checksum(vmem, offset, length);
    vmem->write(vmem, offset+7+length, &checksum, sizeof(checksum));
    // TODO: clear lock
}


void objstore_write_data(vmem_t * vmem, int obj_offset, int data_offset, int data_length, void * data) {
    if (_valid_obj_check(vmem, obj_offset) == 0)
        return;

    // Write the new piece of data
    vmem->write(vmem, obj_offset+data_offset, data, data_length);

    // Create new checksum
    uint16_t obj_length;
    vmem->read(vmem, obj_offset+5, &obj_length, sizeof(obj_length));
    uint8_t checksum = _make_checksum(vmem, obj_offset, obj_length);

    // Write the new checksum
    vmem->write(vmem, obj_offset+7+obj_length, &checksum, sizeof(checksum));
}


/*
static int test_callback(vmem_t * vmem, int offset, int verbose, void * var_in) {

    if (verbose) {
        printf("Found sync-word 5C0FFEE1 at offset %u\n", offset);

        uint8_t type, length;

        vmem->read(vmem, offset+4, &type, 1);
        printf(" object type: %u\n", type);

        vmem->read(vmem, offset+5, &length, 1);
        printf(" object length: %u\n", length);
    }

    return 0;

}
*/

void objstore_init(vmem_t * vmem) {

    vmem_file_init(vmem);

#if 0
    /*
    for (int i = 0; i < NUM_OBJ_TYPES; i++) {
        obj_first_idx[i] = NULL;
    }
    */

    objstore_rm_obj(vmem, 10);

    objstore_scan(vmem, test_callback, 1);

    float test = 5.7;
    int offset = objstore_alloc(vmem, 4);
    if (offset >= 0) {
        objstore_write_obj(vmem, offset, 0, 4, &test);
    }

    float read_test;
    objstore_read_obj(vmem, 10, &read_test);
    printf("Read float %f from vmem\n", read_test);

    //objstore_scan(vmem);

    //objstore_printindex(-1);
#endif
}