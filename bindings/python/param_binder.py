""" This module contains utilities for importing and using the libparam Python bindings with classes. """
# Without special care, as taken by this module, the bindings themselves will act as a singleton.
# Users themselves will also be responsible for calling ._init() on the module if importing and using it manually.

from __future__ import annotations

import sys as _sys
from types import ModuleType as _ModuleType
from importlib import import_module as _import_module
# from typing import Any as _Any


# class _ParamMeta(type):
#
#     def __call__(self, *args: tuple[_Any], **kwds: tuple[_Any]) -> _ModuleType:
#         return super().__call__(*args, **kwds)


# TODO Kevin: Bindings should be a class instead, such that we can use __iter__ and __getitem__.
def Bindings(csp_address: int = 1, csp_version: int = 2, csp_hostname: str = 'python_bindings', csp_model: str = 'linux',
             csp_port: int = 10, *, module_name: str = None) -> _ModuleType:
    """
    Imports and customizes a new instance of the libparam bindings module based on the provided arguments.

    :param csp_address: Which CSP address to use in the module.
    :param csp_port: Which CSP port to use in the module.
    :param module_name: Optional alternative name of the module to import.
    :return: An instance of the libparam binded module on which operations may be performed.
    """

    if not module_name:
        import libparam_py3  # A previous import might be reused if not deleted beforehand.
    else:
        libparam_py3 = _import_module(module_name)

    # Initialize this instance of the module with the provided settings.
    libparam_py3._param_init(csp_address, csp_version, csp_hostname, csp_model, csp_port)

    # Delete the imported module from the cache, to force a new imported next time.
    del _sys.modules[module_name or 'libparam_py3']

    return libparam_py3
