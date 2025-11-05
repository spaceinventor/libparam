#include <vmem/vmem_compress.h>
#include <stddef.h>

#include <errno.h>
#include <sys/types.h>

/*
 * A minimal set of system call stubs for a bare-metal/RTOS system.
 */

// This system has no processes, so there is no process ID.
pid_t getpid(void) {
    return 1;
}

// Kill a process. This is a fatal error in a single-process system.
int kill(pid_t pid, int sig) {
    (void)pid;
    (void)sig;

    // A failed assertion is a critical, unrecoverable error.
    // Halt the system.
    for (;;);

    // Unreachable
    errno = EINVAL;
    return -1;
}

//int miniz_decompressor(uint64_t dst_addr, uint64_t *dst_len, uint64_t src_addr, uint64_t src_len);

vmem_decompress_fnc_t decompress_handler = (vmem_decompress_fnc_t)NULL;
vmem_compress_fnc_t compress_handler = (vmem_decompress_fnc_t)NULL;

void vmem_server_set_decompress_fnc(vmem_decompress_fnc_t fnc) {
    decompress_handler = fnc;
}

void vmem_server_set_compress_fnc(vmem_compress_fnc_t fnc) {
    compress_handler = fnc;
}

vmem_decompress_fnc_t vmem_server_get_decompress_fnc(void) {
    return decompress_handler;
}

vmem_compress_fnc_t vmem_server_get_compress_fnc(void) {
    return compress_handler;
}
