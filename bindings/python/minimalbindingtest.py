from __future__ import annotations

from pyparam_utils import Bindings, temp_autosend_value, ParamMapping
import time


if __name__ == '__main__':

    bindings = Bindings(quiet=False)

    ParamClass = bindings.Parameter
    ParamListClass = bindings.ParameterList

    bindings.autosend(1)

    param = ParamClass("test_array_param")
    # param = ParamClass("csp_rtable")
    # param = ParamClass("col_verbose")

    tempval = param.value

    param.value = reversed(tempval)

    tempval2 = param.value

    exit()

    def param_timit_func():
        param.value = reversed(param.value)

    before = time.time()
    for _ in range(400000):
        param_timit_func()
    print(f"Execution took {(after := (time.time() - before))}")

    # identlst = bindings.ident(1).splitlines()
    # vmemlst = bindings.vmem_list(1).splitlines()

    print("The End")
