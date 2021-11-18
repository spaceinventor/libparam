from __future__ import annotations

import libparam_py3
import param_binder

if __name__ == '__main__':

    bindings = param_binder.Bindings()

    ParamClass = bindings.Parameter
    # paramdict = dict(ParamClass.__dict__)
    # paramdir = dir(ParamClass)

    bindings.node(1)

    # test = bindings.get(200, 1)
    # test2 = bindings.get("col_verbose", 1)

    testparam = ParamClass(202)

    testval = testparam.value
    # testnode = testparam.node

    # testparam.node = 3

    testparam.value = "6"
    # bindings.set(202, "5", 1)

    testval2 = testparam.value
    # testnode2 = testparam.node

    # bindings.ping(1)
    # bindings.ident(1)

    print("The End")
