#include <vmem/vmem_codec.h>
#include <stddef.h>
#include <sys/types.h>

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
