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
    param2 = ParamClass("col_verbose")

    tempval = param.value

    param.value = [1,2,3,"4",5,6,7,8]

    tempval2 = param.value

    param[-1] = param[0]

    tempval3 = param.value
    tempval4 = param[-1]

    tempval5 = param2.value
    param2.value = 5
    tempval6 = param2.value
    param2.value = tempval5

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
