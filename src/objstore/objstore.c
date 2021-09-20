

#include <objstore/objstore.h>

#include <stdio.h>
#include <stdlib.h>


//VMEM_DEFINE_FILE(schedule, "schedule", "schedule.cnf", 0x1000);

const uint8_t sync_word[4] = {0xCA, 0x70, 0xCA, 0xFE};

//typedef int (*objstore_scan_callback_f)(vmem_t * vmem, int offset, int verbose);

#if 0
objstore_idx_t * obj_first_idx[NUM_OBJ_TYPES];

void objstore_printindex(int type) {
    if (type == -1) {
        // print the whole index
        for (int i = 0; i < NUM_OBJ_TYPES; i++) {
            if (obj_first_idx[i]) {
                // first entry
                printf("Object store index, first entry of type %u: offset = %u, length = %u\n", obj_first_idx[i]->type, obj_first_idx[i]->offset, obj_first_idx[i]->length);
                // TODO iterate through linked list for next entries
            }
        }
    } else {
        // print only entries of the requested type
        for (int i = 0; i < NUM_OBJ_TYPES; i++) {
            if (i == type) {
                // first entry
                printf("Object store index, first entry of type %u: offset = %u, length = %u\n", obj_first_idx[i]->type, obj_first_idx[i]->offset, obj_first_idx[i]->length);
                // TODO iterate through linked list for next entries
            }
        }
    }
}
objstore_idx_t * objstore_add_index(uint8_t type, uint16_t offset, uint8_t length) {
    if (type >= NUM_OBJ_TYPES){
        return NULL;
    }

    if (obj_first_idx[type] == NULL) {
        objstore_idx_t * idx_ptr = malloc(sizeof(*idx_ptr));
        if (idx_ptr == NULL)
            return NULL;
        
        idx_ptr->type = (objstore_type_e) type;
        idx_ptr->offset = offset;
        idx_ptr->length = length;
        idx_ptr->ptr_prev = NULL;
        idx_ptr->ptr_next = NULL;

        obj_first_idx[type] = idx_ptr;

        return idx_ptr;
    } else {
        // iterate through the list to find where this new entry fits
        objstore_idx_t * ptr_prev = obj_first_idx[type];
        objstore_idx_t * ptr_next = obj_first_idx[type]->ptr_next;
        while (1) {
            if (ptr_next == NULL) {
                if (ptr_prev->offset < offset){
                    // it's the last entry, simple
                } else {
                    // second to last
                }
            } else {
                if (ptr_next->offset < offset) {
                    // squeeze between next and prev
                } else {
                    // step along the list
                    ptr_prev = ptr_next;
                    ptr_next = ptr_next->ptr_next;
                }
            }
        }
        
    }
    
}
#endif

static uint8_t _make_checksum(void * data, int length) {
    if (length < 0)
        return 0;

    uint8_t checksum = *(uint8_t *) data;
    for (int i = 1; i < length; i++) {
        printf("Creating checksum: %u\n", checksum);
        uint8_t * ptr = (uint8_t *) (long int) (data + i);
        checksum ^= *ptr;
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
    uint16_t data;
    vmem->read(vmem, offset, &data, 2);
    int sync_status = 0;
    if (data == sync_word[0]) {
        sync_status = 1;
        for (int j = 1; j < 4; j++) {
            vmem->read(vmem, offset+j, &data, 1);
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
    vmem->read(vmem, offset+7+length, &checksum, 1);

    if (_valid_checksum(data, length, checksum)) {
        free(data);
        return 1;
    } else {
        free(data);
        return 0;
    }
}

int objstore_alloc(vmem_t * vmem, int length, int verbose) {
    uint8_t sync_status = 0;
    int counter = 0;

    for (int i = 0; i < vmem->size; i++) {
        uint8_t data;
        vmem->read(vmem, i, &data, 1);

        if (data == sync_word[0]) {
            sync_status = 1;
            for (int j = 1; j < 4; j++) {

                vmem->read(vmem, i+j, &data, 1);
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
                    vmem->read(vmem, i+5, &length, 2);
                    i += data+6;
                }
            }
        } else {
            sync_status = 0;
            counter++;
            if ( counter == (length+8) ) {
                if (verbose)
                    printf("Found room for %u bytes plus overhead starting at offset %u\n", length, i-(length+5));
                
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

    vmem->read(vmem, offset+5, &length, 2);

    return length;
}

int objstore_read_obj_type(vmem_t * vmem, int offset) {
    if (_valid_obj_check(vmem, offset) == 0) {
        return -1;
    }
    uint8_t type;

    vmem->read(vmem, offset+4, &type, 1);

    return type;
}

int objstore_scan(vmem_t * vmem, objstore_scan_callback_f callback, int verbose, void * ctx) {
    uint8_t sync_status = 0;
    
    for (int i = 0; i < vmem->size; i++) {
        uint8_t data;
        vmem->read(vmem, i, &data, 1);

        if (data == sync_word[0]) {
            sync_status = 1;
            for (int j = 1; j < 4; j++) {
                vmem->read(vmem, i+j, &data, 1);
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
                    vmem->read(vmem, i+5, &length, 2);
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
    vmem->read(vmem, offset+5, &length, 1);
    // clear data field
    const uint8_t clear_block = 255;
    for (int i = length; i > 0; i--) {
        vmem->write(vmem, offset+i+5, &clear_block, 1);
    }
    // clear header
    vmem->write(vmem, offset+4, &clear_block, 1);
    vmem->write(vmem, offset+5, &clear_block, 1);

    // clear sync word
    for (int i = 0; i < 4; i++) {
        vmem->write(vmem, offset+i, &clear_block, 1);
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
    const uint8_t length;
    vmem->read(vmem, offset+5, &length, 1);

    vmem->read(vmem, offset+6, data_buf, length);

    if (_valid_obj_data_check(vmem, offset, data_buf, length) == 0)
        return -1;

    return 0;
}

void objstore_write_obj(vmem_t * vmem, int offset, uint8_t type, uint16_t length, void * data) {
    // TODO: check for lock
    vmem->write(vmem, offset, sync_word, 4);
    vmem->write(vmem, offset+4, &type, 1);
    vmem->write(vmem, offset+5, &length, 2);
    vmem->write(vmem, offset+7, data, length);

    uint8_t checksum = _make_checksum(data, length);
    printf("Writing object with checksum %u\n", checksum);
    vmem->write(vmem, offset+7+length, &checksum, 1);
    // TODO: clear lock
}

/*
void objstore_write_data(vmem_t * vmem, int offset, int data_offset, int data_length, void * data) {
    if (_valid_obj_check(vmem, offset) == 0)
        return;

    vmem->write(vmem, offset+data_offset, data, data_length);
}
*/

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