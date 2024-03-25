#include <sc/sc_server.h>
#include <param/param_server.h>
#include <csp/csp_types.h>
#include <csp/csp_crc32.h>
#include <csp/csp_hooks.h>

#define SC_CMD_NUM_ELEMENTS 0x80
#define SC_SCH_NUM_ELEMENTS 0x80

#define SC_CMD_BLOCK_SIZE 0x200
#define SC_SCH_BLOCK_SIZE 0x100

#define SEC_TO_NS (uint64_t)1000000000
#define MS_TO_NS (uint64_t)1000000
#define OLD_TIMESTAMP (uint64_t)1000000000000000000


const uint16_t sc_sch_block_size = SC_SCH_BLOCK_SIZE;
const uint16_t sc_cmd_block_size = SC_CMD_BLOCK_SIZE;


// #ifdef __linux__
// // TODO: move these into non-OBC code
// VMEM_DEFINE_FILE(sc_cmd_hash, "sc_ch", "sc_cmd.cnf", sizeof(param_hash_t)*SC_CMD_NUM_ELEMENTS);
// VMEM_DEFINE_FILE(sc_cmd_store, "sc_cs", "sc_cmd_store.cnf", SC_CMD_NUM_ELEMENTS/sizeof(param_hash_t)*SC_CMD_BLOCK_SIZE);
// VMEM_DEFINE_FILE(sc_sch_hash, "sc_sh", "sc_sch.cnf", sizeof(param_hash_t)*SC_SCH_NUM_ELEMENTS);
// VMEM_DEFINE_FILE(sc_sch_store, "sc_ss", "sc_sch_store.cnf", SC_SCH_NUM_ELEMENTS/sizeof(param_hash_t)*SC_SCH_BLOCK_SIZE);

// #else
// #define COMMANDS_FRAM_BASE_ADDR  0x3000
// #define COMMANDS_FRAM_SIZE 0x1000
// VMEM_DEFINE_FRAM(sc_cmd_hash, "sc_ch", VMEM_CONF_CMDV2_FRAM, sizeof(param_hash_t)*SC_CMD_NUM_ELEMENTS, VMEM_CONF_CMDV2_ADDR);
// VMEM_DEFINE_NOR_FLASH(sc_cmd_store, "sc_cs", VMEM_STORE_CMDV2_NOR, SC_CMD_NUM_ELEMENTS/sizeof(param_hash_t)*SC_CMD_BLOCK_SIZE, VMEM_STORE_CMDV2_ADDR);

// #define SCHEDULEV2_FRAM_BASE_ADDR  0x4000
// #define SCHEDULEV2_FRAM_SIZE  0x1000  
// VMEM_DEFINE_FRAM(sc_sch_hash, "sc_sh", VMEM_CONF_SCHV2_FRAM, sizeof(param_hash_t)*SC_SCH_NUM_ELEMENTS, VMEM_CONF_SCHV2_ADDR);
// VMEM_DEFINE_NOR_FLASH(sc_sch_store, "sc_ss", VMEM_STORE_SCHV2_NOR, SC_SCH_NUM_ELEMENTS/sizeof(param_hash_t)*SC_SCH_BLOCK_SIZE, VMEM_STORE_SCHV2_ADDR);
// #endif

typedef void (*param_transaction_callback_f)(csp_packet_t *response, int verbose, int version, void * context);
int param_transaction(csp_packet_t *packet, int host, int timeout, param_transaction_callback_f callback, int verbose, int version, void * context);

typedef enum {
    SCH_STATUS_SCHEDULED = 0,
    SCH_STATUS_COMPLETED = 1,
    SCH_STATUS_FAILED = 2,
    SCH_STATUS_OVERDUE = 3,
    SCH_STATUS_CORRUPTED = 4,
    SCH_STATUS_MISSING_CMD = 5,
    SCH_STATUS_CORRUPTED_CMD = 6,
} sch_status_e;
typedef uint8_t sch_status_t;

typedef struct {
    uint64_t timestamp;
    uint32_t latency_buffer_s;
    param_hash_t cmd_hash;
    uint16_t retries;
    uint8_t status;
} param_sch_element_t;

typedef struct {
    uint64_t timestamp;
    param_hash_t hash;
} sc_execution_t;

/* Time and identity for next schedule element */
static sc_execution_t next_execution;

vmem_t* get_sc_vmem(sc_type_t sc_type) {

    switch (sc_type) {
        case SC_TYPE_CMD:
            return &vmem_sc_cmd_hash;
        case SC_TYPE_SCH:
            return &vmem_sc_sch_hash;
        default:
            return NULL;
        }
}

static int32_t find_addr(param_hash_t hash_in, sc_type_t sc_type) {

    vmem_t* vmem = get_sc_vmem(sc_type);
    int32_t addr = -1;
    param_hash_t hash;

    /* Commands are stored with their hash in one VMEM supporting random access
        A hash value 0 indicate that the particular position is not used
        The actual command is stored in a corresponding position in a seperate VMEM supporting block writes */
    do {
        addr++;
        vmem_memcpy(&hash, vmem->vaddr + addr*sizeof(hash), sizeof(hash));
        if (addr*sizeof(hash) >= vmem_sc_cmd_hash.size) {
            return -1;
        }
    } while (hash != hash_in);

    return addr;
}

static int32_t find_free_slot(sc_type_t sc_type) {

    return find_addr(0, sc_type);
}

static param_hash_t calc_cmd_hash(param_queue_t* queue, char* buffer) {

    char *tmp = queue->buffer;
    // Set to NULL for hash computation as this is an address that may change, while the contents pointed to
    // by this address (which is what the bufffer parameter points to) does not
    queue->buffer = NULL;
    csp_crc32_t crc_obj;
    csp_crc32_init(&crc_obj);
    csp_crc32_update(&crc_obj, (const uint8_t *)queue, sizeof(*queue));
    csp_crc32_update(&crc_obj, (const uint8_t *)buffer, queue->used);
    // Restore the original pointer
    // This dance basically allows computing the hash of a struct, excluding the "queue->buffer" field
    queue->buffer = tmp;
    return csp_crc32_final(&crc_obj);
}

static bool verify_cmd_hash(param_queue_t* queue, uint32_t addr, param_hash_t hash) {

    char queue_buffer[sc_cmd_block_size-sizeof(queue)];
    vmem_memcpy(queue_buffer, vmem_sc_cmd_store.vaddr + addr*sc_cmd_block_size+sizeof(*queue), queue->used);

    csp_crc32_t calc_hash = calc_cmd_hash(queue, queue_buffer);

    if (calc_hash != hash) {
        printf("Command number %"PRIX32" is obstructed (0x%"PRIX32" vs 0x%"PRIX32")\n", addr, hash, calc_hash);
        return false;
    }
    return true;
}

static param_hash_t calc_sch_hash(param_sch_element_t* elm) {

    csp_crc32_t crc_obj;
    csp_crc32_init(&crc_obj);
    csp_crc32_update(&crc_obj, (const uint8_t *)elm, offsetof(param_sch_element_t, retries));
    return csp_crc32_final(&crc_obj);
}

static bool verify_sch_hash(param_sch_element_t* elm, uint32_t addr, param_hash_t hash) {

    csp_crc32_t calc_hash = calc_sch_hash(elm);

    if (calc_hash != hash) {
        printf("Schedule number %"PRIX32" is obstructed (0x%"PRIX32" vs 0x%"PRIX32")\n", addr, hash, calc_hash);
        return false;
    }
    return true;
}

static int sc_forward(param_queue_t * queue) {

    csp_packet_t * fwd = csp_buffer_get(0);
    fwd->data[0] = PARAM_PUSH_REQUEST_V2;
    fwd->data[1] = 0;
    memcpy(&fwd->data[2], queue->buffer, queue->used);
    fwd->length = queue->used + 2;
    fwd->id.pri = CSP_PRIO_HIGH;

    mpack_reader_t reader;
    mpack_reader_init_data(&reader, queue->buffer, queue->used);

    int id, node, offset = -1;
    long unsigned int timestamp = 0;
    param_deserialize_id(&reader, &id, &node, &timestamp, &offset, queue);

    int result = param_transaction(fwd, node, 1000, NULL, 0, 2, NULL);

    return result;
}

void sc_cmd_upload(csp_packet_t * packet) {

    uint16_t unpacked_len = 2;
    csp_packet_t * rsp = csp_buffer_get(0);
    rsp->data[0] = PARAM_COMMAND_UPLOAD_RESPONSE_V2;
    rsp->data[1] = PARAM_FLAG_END;
    rsp->length = 2;

    while (packet->length > unpacked_len) {
        param_cmd_upload_t* cmd = (param_cmd_upload_t*)&packet->data[unpacked_len];
        unpacked_len += sizeof(param_cmd_upload_t) + cmd->param_queue.used;

        cmd->param_queue.buffer = NULL;

        param_sc_rsp_t* rsp_element = (param_sc_rsp_t*)&rsp->data[rsp->length];
        rsp->length += sizeof(param_sc_rsp_t);

        csp_crc32_t *crc_obj = &rsp_element->hash;
        csp_crc32_init(crc_obj);
        csp_crc32_update(crc_obj, (const uint8_t *)&cmd->param_queue, sizeof(cmd->param_queue));
        csp_crc32_update(crc_obj, (const uint8_t *)cmd->param_buffer, cmd->param_queue.used);
        *crc_obj = csp_crc32_final(crc_obj);

        if (cmd->execute) {
            cmd->param_queue.buffer = cmd->param_buffer;
            sc_forward(&cmd->param_queue);
        }
        else {
            rsp_element->result = 0;
        }

        if (rsp_element->result >= 0 && cmd->store) {

            int32_t addr = find_addr(rsp_element->hash, SC_TYPE_CMD);
            if (addr >= 0) {
                if (verify_cmd_hash(&cmd->param_queue, addr, rsp_element->hash)) {
                    printf("Command is already in address %"PRId32"\n", addr);
                    rsp_element->result = 1;
                }
            }
        }

        if (rsp_element->result == 0 && cmd->store) {
            int32_t addr = find_free_slot(SC_TYPE_CMD);
            if (addr < 0) {
                rsp_element->result = -1;
            } else {
                vmem_memcpy(vmem_sc_cmd_hash.vaddr+addr*sizeof(rsp_element->hash), &rsp_element->hash, sizeof(&rsp_element->hash));
                vmem_memcpy(vmem_sc_cmd_store.vaddr+addr*sc_cmd_block_size, &cmd->param_queue, sizeof(cmd->param_queue));
                vmem_memcpy(vmem_sc_cmd_store.vaddr+addr*sc_cmd_block_size+sizeof(cmd->param_queue), cmd->param_buffer, cmd->param_queue.used);
            }
        }
    }

    csp_sendto_reply(packet, rsp, CSP_O_SAME);
}

void sc_cmd_execute(csp_packet_t * packet) {

    uint16_t unpacked_len = 2;
    csp_packet_t * rsp = csp_buffer_get(0);
    rsp->data[0] = PARAM_COMMAND_EXECUTE_RESPONSE_V2;
    rsp->data[1] = PARAM_FLAG_END;
    rsp->length = 2;

    while (packet->length > unpacked_len) {

        param_sc_req_t* cmd = (param_sc_req_t*)&packet->data[unpacked_len];
        unpacked_len += sizeof(*cmd);

        param_sc_rsp_t* rsp_element = (param_sc_rsp_t*)&rsp->data[rsp->length];
        rsp->length += sizeof(param_sc_rsp_t);
        rsp_element->hash = cmd->hash;

        param_hash_t hash = 0;

        int32_t addr = find_addr(cmd->hash, SC_TYPE_CMD);
        if (addr < 0) {
            rsp_element->result = -1;
        }

        if (rsp_element->result >= 0) {
            param_queue_t queue;
            vmem_memcpy(&queue, vmem_sc_cmd_store.vaddr+addr*sc_cmd_block_size, sizeof(queue));

            rsp_element->result = verify_cmd_hash(&queue, addr, hash);

            if (rsp_element->result < 0) {
                continue;
            }

            rsp_element->result = sc_forward(&queue);
        }
    }

    csp_sendto_reply(packet, rsp, CSP_O_SAME);
}

void sc_cmd_list(csp_packet_t * packet) {

    csp_packet_t * rsp = csp_buffer_get(0);
    rsp->data[0] = PARAM_COMMAND_LIST_RESPONSE_V2;
    rsp->data[1] = PARAM_FLAG_END;
    rsp->length = 2;

    param_hash_t hash = 0;
    for (int32_t addr = 0; addr < vmem_sc_cmd_hash.size/sizeof(param_hash_t); addr++) {

        vmem_memcpy(&hash, vmem_sc_cmd_hash.vaddr+addr*sizeof(param_hash_t), sizeof(hash));

        if (hash != 0) {

            param_sc_rsp_t* rsp_element = (param_sc_rsp_t*)&rsp->data[rsp->length];
            rsp->length += sizeof(param_sc_rsp_t);

            rsp_element->hash = hash;
            rsp_element->result = 0;

            param_queue_t queue;
            vmem_memcpy(&queue, vmem_sc_cmd_store.vaddr+addr*sc_cmd_block_size, sizeof(queue));
            if (!verify_cmd_hash(&queue, addr, hash)) {
                hash = 0;
                rsp_element->result = -1;
                vmem_memcpy(vmem_sc_cmd_hash.vaddr+addr*sizeof(param_hash_t), &hash, sizeof(hash));
                printf("Found invalid hash in addr %"PRId32", resetting\n", addr);
            }
        }
    }

    csp_sendto_reply(packet, rsp, CSP_O_SAME);
}

void sc_cmd_download(csp_packet_t * packet) {

    uint16_t unpacked_len = 2;
    csp_packet_t * rsp = csp_buffer_get(0);
    rsp->data[0] = PARAM_COMMAND_DOWNLOAD_RESPONSE_V2;
    rsp->data[1] = PARAM_FLAG_END;
    rsp->length = 2;

    while (packet->length > unpacked_len) {

        param_sc_req_t* cmd = (param_sc_req_t*)&packet->data[unpacked_len];
        unpacked_len += sizeof(*cmd);

        param_hash_t hash = 0;
        for (int32_t addr = 0; addr < vmem_sc_cmd_hash.size/sizeof(param_hash_t); addr++) {
            vmem_memcpy(&hash, vmem_sc_cmd_hash.vaddr+addr*sizeof(hash), sizeof(hash));
            if (hash != 0 && (cmd->hash == hash || cmd->hash == 0)) {

                param_cmd_download_t* rsp_element = (param_cmd_download_t*)&rsp->data[rsp->length];

                vmem_memcpy(&rsp_element->param_queue, vmem_sc_cmd_store.vaddr+addr*sc_cmd_block_size, sizeof(rsp_element->param_queue));
                vmem_memcpy(rsp_element->param_buffer, vmem_sc_cmd_store.vaddr+addr*sc_cmd_block_size+sizeof(rsp_element->param_queue), rsp_element->param_queue.used);
                rsp->length += sizeof(param_cmd_download_t) + rsp_element->param_queue.used;
                if (cmd->hash != 0) break;
            }
        }
    }

    csp_sendto_reply(packet, rsp, CSP_O_SAME);
}

void sc_remove(csp_packet_t * packet) {

    uint16_t unpacked_len = 2;
    csp_packet_t * rsp = csp_buffer_get(0);
    rsp->data[0] = packet->data[0] == PARAM_COMMAND_REMOVE_REQUEST_V2 ? PARAM_COMMAND_REMOVE_RESPONSE_V2 : PARAM_SCHEDULE_REMOVE_RESPONSE_V2;
    rsp->data[1] = PARAM_FLAG_END;
    rsp->length = 2;

    while (packet->length > unpacked_len) {

        param_sc_req_t* cmd = (param_sc_req_t*)&packet->data[unpacked_len];
        unpacked_len += sizeof(*cmd);

        vmem_t* vmem = NULL;
        switch (packet->data[0]) {
            case PARAM_COMMAND_REMOVE_REQUEST_V2:
                vmem = &vmem_sc_cmd_hash;
                break;
            case PARAM_SCHEDULE_REMOVE_REQUEST_V2:
                vmem = &vmem_sc_sch_hash;
                break;
        }

        if (vmem == NULL) {
            break;
        }

        param_hash_t hash = 0;
        if (cmd->hash == 0) {
            /* Remove all entries */
            for (uint32_t addr = 0; addr < vmem->size/sizeof(param_hash_t); addr++) {
                vmem_memcpy(&hash, vmem->vaddr+addr*sizeof(hash), sizeof(hash));
                if (hash != 0) {
                    hash = 0;
                    vmem_memcpy(vmem->vaddr+addr*sizeof(hash), &hash, sizeof(hash));
                }
            }
        }
        else {
            /* Remove specific entry */
            for (uint32_t addr = 0; addr < vmem->size; addr += sizeof(param_hash_t)) {
                vmem_memcpy(&hash, vmem->vaddr+addr*sizeof(hash), sizeof(hash));
                if (hash == cmd->hash) {
                    hash = 0;
                    vmem_memcpy(vmem->vaddr+addr*sizeof(hash), &hash, sizeof(hash));
                    break;
                }
            }
        }
    }    

    csp_sendto_reply(packet, rsp, CSP_O_SAME);
}

sc_execution_t sc_next_execution() {

    csp_timestamp_t time_v;
    csp_clock_get_time(&time_v);
    uint64_t time = time_v.tv_sec*SEC_TO_NS + time_v.tv_nsec;

    sc_execution_t next = {.timestamp = UINT64_MAX};

    for (int32_t addr = 0; addr < vmem_sc_sch_hash.size/sizeof(param_hash_t); addr++) {
        param_hash_t hash;
        vmem_memcpy(&hash, vmem_sc_sch_hash.vaddr+addr*sizeof(hash), sizeof(hash));
        if (hash != 0) {
            param_sch_element_t elm;
            vmem_memcpy(&elm, vmem_sc_sch_store.vaddr+addr*sc_sch_block_size, sizeof(elm));
            if (elm.status == SCH_STATUS_SCHEDULED && elm.timestamp < next.timestamp) {
                if (elm.latency_buffer_s > 0 && elm.timestamp + elm.latency_buffer_s*SEC_TO_NS < time) {
                    elm.status = SCH_STATUS_OVERDUE;
                    vmem_memcpy(vmem_sc_sch_store.vaddr+addr*sc_sch_block_size, &elm, sizeof(elm));
                } else {
                    next.timestamp = elm.timestamp;
                    next.hash = hash;
                }
            }
        }
    }

    printf("Scheduling 0x%"PRIX32" in %"PRIu64" sec\n", next.hash, (next.timestamp - time)/SEC_TO_NS);

    return next;
}

static int8_t sc_schedule(param_hash_t cmd_hash, uint32_t timestamp_s, uint32_t latency_buffer_s, param_hash_t* sch_hash) {

    param_sch_element_t elm = {
        .timestamp = timestamp_s * SEC_TO_NS,
        .latency_buffer_s = latency_buffer_s,
        .retries = 5,
        .cmd_hash = cmd_hash,
        .status = SCH_STATUS_SCHEDULED,
    };

    csp_timestamp_t time;
    csp_clock_get_time(&time);

    /* Accept relative timestamps recognized by UTC timestamps in past */
    if (elm.timestamp < OLD_TIMESTAMP) {
        elm.timestamp += time.tv_sec*SEC_TO_NS + time.tv_nsec;
    }

    if (elm.timestamp < time.tv_sec*SEC_TO_NS + time.tv_nsec) {
        return -2;
    }

    *sch_hash = calc_sch_hash(&elm);
    int32_t sch_addr = find_addr(*sch_hash, SC_TYPE_SCH);

    if (sch_addr < 0) {
        sch_addr = find_free_slot(SC_TYPE_SCH);
        if (sch_addr < 0) {
            return -3;
        }
        vmem_memcpy(vmem_sc_sch_hash.vaddr+sch_addr*sizeof(*sch_hash), sch_hash, sizeof(*sch_hash));
        vmem_memcpy(vmem_sc_sch_store.vaddr+sch_addr*sc_sch_block_size, &elm, sizeof(elm));
        next_execution = sc_next_execution();
        return 0;
    } else {
        vmem_memcpy(vmem_sc_sch_store.vaddr+sch_addr*sc_sch_block_size, &elm, sizeof(elm));
        return 1;
    }
}

void sc_sch_push(csp_packet_t * packet) {

    uint16_t unpacked_len = 2;
    csp_packet_t * rsp = csp_buffer_get(0);
    rsp->data[0] = PARAM_SCHEDULE_PUSH_RESPONSE_V2;
    rsp->data[1] = PARAM_FLAG_END;
    rsp->length = 2;

    while (packet->length > unpacked_len) {

        param_sch_push_t* cmd = (param_sch_push_t*)&packet->data[unpacked_len];
        unpacked_len += sizeof(*cmd) + cmd->param_queue.used;

        param_sc_rsp_t* rsp_element = (param_sc_rsp_t*)&rsp->data[rsp->length];
        rsp->length += sizeof(*rsp_element);

        param_hash_t cmd_hash = calc_cmd_hash(&cmd->param_queue, cmd->param_buffer);

        if (find_addr(cmd_hash, SC_TYPE_CMD) < 0) {
            int32_t cmd_addr = find_free_slot(SC_TYPE_CMD);

            if (cmd_addr < 0) {
                rsp_element->result = -1;
                continue;
            } else {
                vmem_memcpy(vmem_sc_cmd_hash.vaddr+cmd_addr*sizeof(cmd_hash), &cmd_hash, sizeof(&cmd_hash));
                vmem_memcpy(vmem_sc_cmd_store.vaddr+cmd_addr*sc_cmd_block_size, &cmd->param_queue, sizeof(cmd->param_queue));
                vmem_memcpy(vmem_sc_cmd_store.vaddr+cmd_addr*sc_cmd_block_size+sizeof(cmd->param_queue), cmd->param_buffer, cmd->param_queue.used);
            }
        }

        sc_schedule(cmd_hash, cmd->timestamp_s, cmd->latency_buffer_s, &rsp_element->hash);
    }    

    csp_sendto_reply(packet, rsp, CSP_O_SAME);
}

void sc_sch_command(csp_packet_t * packet) {

    uint16_t unpacked_len = 2;
    csp_packet_t * rsp = csp_buffer_get(0);
    rsp->data[0] = PARAM_SCHEDULE_COMMAND_RESPONSE_V2;
    rsp->data[1] = PARAM_FLAG_END;
    rsp->length = 2;

    while (packet->length > unpacked_len) {

        param_sch_command_t* cmd = (param_sch_command_t*)&packet->data[unpacked_len];
        unpacked_len += sizeof(*cmd);

        param_sc_rsp_t* rsp_element = (param_sc_rsp_t*)&rsp->data[rsp->length];
        rsp->length += sizeof(*rsp_element);

        if (find_addr(cmd->command_hash, SC_TYPE_CMD) < 0) {
            rsp_element->result = -1;
            continue;
        }

        sc_schedule(cmd->command_hash, cmd->timestamp_s, cmd->latency_buffer_s, &rsp_element->hash);
    }    

    csp_sendto_reply(packet, rsp, CSP_O_SAME);
}

void sc_tick(csp_timestamp_t time_v, uint32_t periodicity_ms) {

    uint64_t time = time_v.tv_sec*SEC_TO_NS + time_v.tv_nsec;

    while (next_execution.timestamp < time + periodicity_ms*MS_TO_NS) {

        /* Find address of schedule element */
        int32_t sch_addr = find_addr(next_execution.hash, SC_TYPE_SCH);
        if (sch_addr < 0) {
            goto end;
        }

        /* Retrieve schedule element and check if time is exceeded */
        param_sch_element_t elm;
        vmem_memcpy(&elm, vmem_sc_sch_store.vaddr+sch_addr*sc_sch_block_size, sizeof(elm));

        /* Check if SCH hash is valid */
        if (!verify_sch_hash(&elm, sch_addr, next_execution.hash)) {
            next_execution.hash = 0;
            vmem_memcpy(vmem_sc_sch_hash.vaddr+sch_addr*sizeof(param_hash_t), &next_execution.hash, sizeof(param_hash_t));
            goto end;
        }

        /* Check if we are already too late */
        if (elm.latency_buffer_s > 0 && elm.timestamp + elm.latency_buffer_s * SEC_TO_NS <= time) {
            elm.status = SCH_STATUS_OVERDUE;
            vmem_memcpy(vmem_sc_sch_store.vaddr+sch_addr*sc_sch_block_size, &elm, sizeof(elm));
            goto end;
        }

        /* Check if command hash points to a valid command */
        int32_t cmd_addr = find_addr(elm.cmd_hash, SC_TYPE_CMD);
        if (cmd_addr < 0) {
            elm.status = SCH_STATUS_MISSING_CMD;
            vmem_memcpy(vmem_sc_sch_store.vaddr+sch_addr*SC_SCH_BLOCK_SIZE, &elm, sizeof(elm));
            goto end;
        }

        /* Verify that command hash is valid */
        param_queue_t queue;
        vmem_memcpy(&queue, vmem_sc_cmd_store.vaddr+cmd_addr*sc_cmd_block_size, sizeof(queue));
        if (!verify_cmd_hash(&queue, cmd_addr, elm.cmd_hash)) {
            elm.status = SCH_STATUS_CORRUPTED_CMD;
            vmem_memcpy(vmem_sc_sch_store.vaddr+sch_addr*SC_SCH_BLOCK_SIZE, &elm, sizeof(elm));
            goto end;
        }

        {
            /* Retrieve command param buffer */
            char queue_buffer[sc_cmd_block_size-sizeof(queue)];
            vmem_memcpy(queue_buffer, vmem_sc_cmd_store.vaddr+cmd_addr*sc_cmd_block_size+sizeof(queue), queue.used);
            queue.buffer = queue_buffer;

            /* Forward the set command and set schedule element status */
            printf("Forwarding %s\n", queue.name);
            if (sc_forward(&queue) < 0) {
                elm.status = SCH_STATUS_FAILED;
            } else {
                elm.status = SCH_STATUS_COMPLETED;
            }
            vmem_memcpy(vmem_sc_sch_store.vaddr+sch_addr*SC_SCH_BLOCK_SIZE, &elm, sizeof(elm));
        }

end:
        next_execution = sc_next_execution();
    }
}

// TODO: move into non-OBC code
void sc_init() {

#ifdef __linux__
	vmem_file_init(&vmem_sc_cmd_hash);
	vmem_file_init(&vmem_sc_cmd_store);
	vmem_file_init(&vmem_sc_sch_hash);
	vmem_file_init(&vmem_sc_sch_store);
#endif
    next_execution = sc_next_execution();
}
