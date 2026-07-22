#include <vmem/vmem_file.h>

VMEM_DEFINE_FILE_VADDR(plt_405, "plt_405", "plt_405.vmem", 1024, 0x5000);

static const char test_vector[] = "This is a test";

int main (int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    vmem_write(vmem_plt_405.vaddr, test_vector, sizeof(test_vector));
    return 1;
}