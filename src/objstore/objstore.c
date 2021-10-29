

#include <objstore/objstore.h>

#include <stdio.h>
#include <stdlib.h>

const uint8_t sync_word[4] = {0xCA, 0x70, 0xCA, 0xFE};

static uint8_t _make_checksum(vmem_t * vmem, int offset, int length) {
    if (length < 0)
        return 0;

    uint8_t buf, checksum;
    vmem->read(vmem, offset+OBJ_HEADER_LENGTH, &checksum, sizeof(checksum)); // initialize with first data byte
    for (int i = 1; i < length; i++) {
        vmem->read(vmem, offset+OBJ_HEADER_LENGTH+i, &buf, sizeof(buf));
        checksum ^= buf;
    }

    return checksum;
}

static int _valid_checksum(vmem_t * vmem, int offset, uint16_t length, uint8_t checksum) {
    if (length < 0)
        return 0;
    
    uint8_t temp_checksum = _make_checksum(vmem, offset, length);

    if (checksum == temp_checksum) {
        return 1;
    } else {
        return 0;
    }
}

static int _valid_obj_check(vmem_t * vmem, int offset) {
    uint8_t data;
    int sync_status = 0;
    for (int i = 0; i < 4; i++) {
        vmem->read(vmem, offset+i, &data, sizeof(data));
        if (data == sync_word[sync_status]) {
            sync_status++;
        } else {
            return 0;
        }
    }

    return 1;
}

static int _valid_obj_data_check(vmem_t * vmem, int offset, uint16_t length) {
    uint8_t checksum;
    vmem->read(vmem, offset+OBJ_HEADER_LENGTH+length, &checksum, sizeof(checksum));

    if (_valid_checksum(vmem, offset, length, checksum)) {
        return 1;
    } else {
        return 0;
    }
}

int objstore_read_obj_length(vmem_t * vmem, int offset) {
    if ( (offset < 0) || (_valid_obj_check(vmem, offset) == 0) ) {
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

                    
                    // read length to skip ahead
                    int length = objstore_read_obj_length(vmem, i);
                    if (length < 0) {
                        i += OBJ_HEADER_LENGTH;
                        break;
                    }
                    //printf("objstore_alloc skipping object of length: %u\n", length);
                    i += length + OBJ_HEADER_LENGTH;
                }
            }
        } else {
            sync_status = 0;
            counter++;
            if ( counter == (length+OBJ_HEADER_LENGTH+1) ) {
                if (verbose)
                    printf("Found room for %u bytes plus overhead starting at offset %u\n", length, i-(length+OBJ_HEADER_LENGTH));
                
                // todo: Lock the allocated piece of memory
                return i-(length+OBJ_HEADER_LENGTH);
            }
        }
    }

    return -1;
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
                    i += length+OBJ_HEADER_LENGTH-1;
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
    vmem->read(vmem, offset+5, (void *) &length, sizeof(length));

    const uint8_t clear_block = 0xFF;
    // clear checksum
    vmem->write(vmem, offset+length+OBJ_HEADER_LENGTH, (void *) &clear_block, sizeof(clear_block));
    // clear data field
    for (int i = length; i > 0; i--) {
        vmem->write(vmem, offset+i+OBJ_HEADER_LENGTH-1, (void *) &clear_block, sizeof(clear_block));
    }
    // clear header
    vmem->write(vmem, offset+6, (void *) &clear_block, sizeof(clear_block));
    vmem->write(vmem, offset+5, (void *) &clear_block, sizeof(clear_block));
    vmem->write(vmem, offset+4, (void *) &clear_block, sizeof(clear_block));
    // clear sync word
    for (int i = 0; i < 4; i++) {
        vmem->write(vmem, offset+i, (void *) &clear_block, sizeof(clear_block));
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
    vmem->read(vmem, offset+5, (void *) &length, sizeof(length));

    vmem->read(vmem, offset+OBJ_HEADER_LENGTH, data_buf, length);

    if (_valid_obj_data_check(vmem, offset, length) == 0)
        return -2;

    return 0;
}

void objstore_write_obj(vmem_t * vmem, int offset, uint8_t type, uint16_t length, void * data) {
    vmem->write(vmem, offset, (void *) sync_word, 4);
    vmem->write(vmem, offset+4, &type, sizeof(type));
    vmem->write(vmem, offset+5, &length, sizeof(length));
    vmem->write(vmem, offset+OBJ_HEADER_LENGTH, data, length);

    uint8_t checksum = _make_checksum(vmem, offset, length);
    vmem->write(vmem, offset+OBJ_HEADER_LENGTH+length, &checksum, sizeof(checksum));
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
    vmem->write(vmem, obj_offset+OBJ_HEADER_LENGTH+obj_length, &checksum, sizeof(checksum));
}
