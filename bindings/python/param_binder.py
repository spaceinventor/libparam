""" This module contains utilities for importing and using the libparam Python bindings with classes. """
# Without special care, as taken by this module, the bindings themselves will act as a singleton.
# Users themselves will also be responsible for calling ._init() on the module if importing and using it manually.

from __future__ import annotations

import os as _os
import sys as _sys
import libparam_py3
from typing import Any as _Any
from shutil import copyfile as _copyfile
from types import ModuleType as _ModuleType
from importlib import import_module as _import_module

# class _ParamMeta(type):
#
#     def __call__(self, *args: tuple[_Any], **kwds: tuple[_Any]) -> _ModuleType:
#         return super().__call__(*args, **kwds)


# class BindingsHelper:
#     """ Wrapper for operations performed on the bindings extension module. Allows for overriding __iter__ and such. """
#     # This class could be implemented in the C extension module instead, but most sources advise against this.
#     # Considering that the extension module is a dynamic library as well, this could would likely be a singleton too.
#
#     _module: _ModuleType
#     __module_counter = 0
#
#     def __init__(self, csp_address: int = 1, csp_version: int = 2, csp_hostname: str = 'python_bindings',
#                  csp_model: str = 'linux', csp_port: int = 10, *, module_name: str = None) -> None:
#         """
#         Imports and customizes a new instance of the libparam bindings module based on the provided arguments.
#
#         :param csp_address: Which CSP address to use in the module.
#         :param csp_port: Which CSP port to use in the module.
#         :param module_name: Optional alternative name of the module to import.
#         :return: An instance of the libparam binded module on which operations may be performed.
#         """
#         super(BindingsHelper, self).__init__()
#
#         if not module_name:
#             import libparam_py3  # A previous import might be reused if not deleted beforehand.
#         else:
#             libparam_py3 = _import_module(module_name)
#
#         source = libparam_py3.__file__
#         destination = f"{source.split('.')[0]}_{type(self).__module_counter}.{'.'.join(source.split('.')[1:])}"
#
#         # Delete the imported module from the cache, to force a new imported next time.
#         del _sys.modules[module_name or 'libparam_py3']
#
#         _copyfile(source, destination)
#
#         libparam_py3_copy = _import_module(f"{module_name or 'libparam_py3'}_{type(self).__module_counter}")
#
#         type(self).__module_counter += 1
#
#         self._module = libparam_py3_copy
#
#         # Initialize this instance of the module with the provided settings.
#         libparam_py3_copy._param_init(csp_address, csp_version, csp_hostname, csp_model, csp_port)
#
#     def __getattribute__(self, name: str) -> _Any:
#         """ Forward attribute retrievals to the stored library. """
#         return getattr(super(BindingsHelper, self).__getattribute__('_module'), name)
#
#     def __iter__(self):
#         """ Iterates over all known parameters. """
#         raise NotImplementedError("Cannot iterate parameters yet")
#         for param in self.list_parameters():
#             yield param
#
#     def __del__(self):
#         _os.remove(f"{super(BindingsHelper, self).__getattribute__('_module').__file__}")


# TODO Kevin: Bindings should be a class instead, such that we can use __iter__ and __getitem__.
def Bindings(csp_address: int = ..., csp_version: int = ..., csp_hostname: str = ..., csp_model: str = ...,
             csp_port: int = ..., can_dev: str = ..., *, module_name: str = None) -> _libparam_typehint:
    """
    Imports and customizes a new instance of the libparam bindings module based on the provided arguments.

    :param csp_address: Which CSP address to use in the module.
    :param csp_version: Which CSP version to use in the module.
    :param csp_hostname: Which CSP hostname to use in the module.
    :param csp_model: Which CSP model to use in the module.
    :param csp_port: Which CSP port to use in the module.
    :param module_name: Optional alternative name of the module to import.
    :param can_dev: Can interface to use.
    :return: An instance of the libparam binded module on which operations may be performed.
    """

    init_dict = {key: value for key, value in
                 {
                     'csp_address': csp_address,
                     'csp_version': csp_version,
                     'csp_hostname': csp_hostname,
                     'csp_model': csp_model,
                     'csp_port': csp_port,
                     'can_dev': can_dev
                 }.items()
                 if value is not ...}

    if not module_name:
        import libparam_py3  # A previous import might be reused if not deleted beforehand.
    else:
        libparam_py3 = _import_module(module_name)

    # Initialize this instance of the module with the provided settings.
    libparam_py3._param_init(**init_dict)

    # Delete the imported module from the cache, to force a new import next time.
    del _sys.modules[module_name or 'libparam_py3']

    return libparam_py3
