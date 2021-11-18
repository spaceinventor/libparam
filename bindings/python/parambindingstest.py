from __future__ import annotations

import param_binder
# from multiprocessing.synchronize import Event as EventHint
# from multiprocessing import Process, Event
#
#
# def pull_param_async(param_identifier: int | str, startevent: EventHint):
#     bindings = param_binder.Bindings()
#     while not startevent.is_set():
#         time.sleep(1)
#
#     bindings.param_push_single(param_identifier, "", 1)
#
#
# ident = 12
#
# startevent = Event()
#
# bindings = [Process(target=pull_param_async, args=(ident, startevent)) for _ in range(3)]
#
# for process in bindings:
#     process.start()
#
# startevent.set()
#
# for process in bindings:
#     process.join()
#
#
# raise SystemExit
#
libparam = param_binder.Bindings(module_name='libparam_py3')

# libparam.ping(1)
# libparam.ident(1)

test = libparam.get(200, 1)
test2 = libparam.get("col_verbose", 1)
print(test)
print(test2)

libparam.set("col_cnfstr", "test", 1)
libparam.set(202, "55", 1)

test3 = libparam.get(200, 1)
test4 = libparam.get("col_verbose", 1)
print(test3)
print(test4)

raise SystemExit

# modules = []
# for i in range(10):
#     module = param_binder.Bindings()
#     module.testvar = i
#     modules.append(module)
#
# for index, module in enumerate(modules):
#     print(module.testvar)
#     try:
#         module.param_push_single(index)
#     except ValueError:
#         continue
#
# raise SystemExit


import libparam_py3 as libparam
# print(dir(libparam))
libparam.param_push_single(12)
