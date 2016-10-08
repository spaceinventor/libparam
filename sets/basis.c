#include <stdint.h>
#include <stddef.h>

#include <param.h>
#include <vmem_fram.h>
#include <vmem_fram_secure.h>

#include <param/basis.h>

static void basis_param_fallback(void) {
	param_set_uint8(&basis_param.csp_node, 0);
	param_set_uint32(&basis_param.csp_can_speed, 1000000);
}

VMEM_DEFINE_FRAM_SECURE(basis, "basis", 0x0000, 0x6000, basis_param_fallback, 0x54, 0x31000000)

basis_param_t basis_param __attribute__ ((section("param"), used)) = {
	PARAM_DEFINE_STRUCT_VMEM(csp_node, PARAM_TYPE_UINT8, -1, 0, 32, PARAM_READONLY_FALSE, NULL, "", basis, 0x00), \
	PARAM_DEFINE_STRUCT_VMEM(csp_can_speed, PARAM_TYPE_UINT32, -1, 128000, 1000000, PARAM_READONLY_FALSE, NULL, "", basis, 0x04), \
	PARAM_DEFINE_STRUCT_VMEM(csp_rtable, PARAM_TYPE_STRING, 64, -1, -1, PARAM_READONLY_FALSE, NULL, "", basis, 0x10), \
};

#undef PARAM_SET


