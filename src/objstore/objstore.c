

#include "objstore.h"

#include <stdio.h>
#include <stdlib.h>


//VMEM_DEFINE_FILE(schedule, "schedule", "schedule.cnf", 0x1000);

const uint8_t sync_word[4] = {0x5C, 0x0F, 0xFE, 0xE1};

typedef void (*objstore_scan_callback_f)(vmem_t * vmem, int offset, int verbose);

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

int objstore_alloc(vmem_t * vmem, int length) {
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

                    // read length to skip ahead
                    vmem->read(vmem, i+5, &data, 1);
                    i += data+5;
                }
            }
        } else {
            sync_status = 0;
            counter++;
            if ( counter == (length+6) ) {
                printf("Found room for %u bytes plus overhead starting at offset %u\n", length, i-(length+5));
                // todo: Lock the allocated piece of memory
                return i-(length+5);
            }
        }
    }

    return -1;

}

void objstore_scan(vmem_t * vmem, objstore_scan_callback_f callback, int verbose) {
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

                    callback(vmem, i, verbose);
                    
                    sync_status = 0;

                    uint8_t length;
                    vmem->read(vmem, i+5, &length, 1);
                    i += length+5;
                }
            }
        } else {
            sync_status = 0;
        }
    }

}

int objstore_rm_obj(vmem_t * vmem, int offset, int verbose) {
    // check that a valid object is actually present
    uint8_t data;
    vmem->read(vmem, offset, &data, 1);
    int sync_status = 0;
    if (data == sync_word[0]) {
        sync_status = 1;
        for (int j = 1; j < 4; j++) {
            vmem->read(vmem, offset+j, &data, 1);
            if (data == sync_word[sync_status]) {
                sync_status++;
            } else {
                if (verbose)
                    printf("Obj deletion error! No valid object found at offset %u\n", offset);
                
                return -1;
            }
        }
    } else {
        if (verbose)
            printf("Obj deletion error! No valid object found at offset %u\n", offset);
        
        return -1;
    }

    const uint8_t length;
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

int objstore_read_obj(vmem_t * vmem, int offset, void * data_buf) {
    // check that a valid object is actually present
    uint8_t data;
    vmem->read(vmem, offset, &data, 1);
    int sync_status = 0;
    if (data == sync_word[0]) {
        sync_status = 1;
        for (int j = 1; j < 4; j++) {
            vmem->read(vmem, offset+j, &data, 1);
            if (data == sync_word[sync_status]) {
                sync_status++;
            } else {
                printf("Obj read error! No valid object found at offset %u\n", offset);
                return -1;
            }
        }
    } else {
        printf("Obj read error! No valid object found at offset %u\n", offset);
        return -1;
    }

    // data buffer must already be the right size
    const uint8_t length;
    vmem->read(vmem, offset+5, &length, 1);

    vmem->read(vmem, offset+6, data_buf, length);

    return 0;
}

void objstore_write_obj(vmem_t * vmem, int offset, uint8_t type, uint8_t length, void * data) {
    // TODO: check for lock
    vmem->write(vmem, offset, sync_word, 4);
    vmem->write(vmem, offset+4, &type, 1);
    vmem->write(vmem, offset+5, &length, 1);
    vmem->write(vmem, offset+6, data, length);
    // TODO: clear lock
}

static void test_callback(vmem_t * vmem, int offset, int verbose) {

    if (verbose) {
        printf("Found sync-word 5C0FFEE1 at offset %u\n", offset);

        uint8_t type, length;

        vmem->read(vmem, offset+4, &type, 1);
        printf(" object type: %u\n", type);

        vmem->read(vmem, offset+5, &length, 1);
        printf(" object length: %u\n", length);
    }

}

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