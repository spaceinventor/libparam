#include <assert.h>
#include <string.h>
#include <vmem/vmem_file.h>

VMEM_DEFINE_FILE_VADDR(plt_405, "plt_405", "plt_405.vmem", 1024, 0x5000);

static const char test_vector[] = "This is a test";

int main (int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    char buf[sizeof(test_vector)] = {0};
    vmem_write(vmem_plt_405.vaddr, test_vector, sizeof(test_vector));
    vmem_read(buf, vmem_plt_405.vaddr, sizeof(test_vector));
    assert(0 == memcmp(buf, test_vector, sizeof(test_vector)));
    return 1;
}