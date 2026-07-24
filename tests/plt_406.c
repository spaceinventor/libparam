#include <assert.h>
#include <string.h>
#include <param/param.h>
#include <param/param_list.h>

static int32_t _plt_406;

PARAM_DEFINE_STATIC_RAM(1, plt_406, PARAM_TYPE_INT32, 0, 1, PM_READONLY, NULL, "-", &_plt_406, "PLT 406 test parameter");

int main (int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    /* Make sure our own param (id = 1) is in the list */
    const param_t * p = param_list_find_id(0, 1);
    if(NULL == p) {
        /* libparam is probably built as a shared library -> need to explicitly add our own parameters*/
        param_list_add((param_t *)&plt_406);
    }
    p = param_list_find_id(0, 1);
    assert(p);

    /* Add a remote parameter with node = 0 */
    param_t *remote = param_list_create_remote(128, 0, PARAM_TYPE_UINT16, PM_DEBUG, 0, "remote", NULL, NULL, -1);
    assert(p);
    param_list_add(remote);
    p = param_list_find_id(0, 128);
    assert(p);

    /* Add a remote parameter with node != 0 */
    remote = param_list_create_remote(128, 400, PARAM_TYPE_UINT16, PM_DEBUG, 0, "remote", NULL, NULL, -1);
    assert(remote);
    param_list_add(remote);
    p = param_list_find_id(400, 128);
    assert(p);

    /* Clear the list */
    param_list_clear();

    /* Can we still find our own added remote parameter with node = 0 ? */
    p = param_list_find_id(0, 128);
    assert(p);

    /* Did we remove the remote parameter (node != 0) from the list ? */
    p = param_list_find_id(400, 128);
    assert(NULL == p);

    /* Explicitly remove our own remote parameter with node = 0 ? */
    p = param_list_find_id(0, 128);
    assert(p);
    param_list_remove_specific(p, 0, 1);
    p = param_list_find_id(0, 128);
    assert(NULL == p);

    /* Can we still find our local parameter ? */
    p = param_list_find_id(0, 1);
    assert(p);
    return 0;
}