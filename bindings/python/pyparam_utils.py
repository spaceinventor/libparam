""" This module contains utilities for importing, initializing and using the libparam Python bindings module. """
# Without special care, as taken by this module, users themselves will be responsible for calling ._init()
# on the libparam bindings module if importing and using it manually.

from __future__ import annotations

import sys as _sys
import libparam_py3
from importlib import import_module as _import_module
from contextlib import contextmanager as _contextmanager

_libparam_typehint = libparam_py3


# TODO Kevin: Bindings should be a class instead, such that we can use __iter__ and __getitem__.
def Bindings(csp_address: int = ..., csp_version: int = ..., csp_hostname: str = ..., csp_model: str = ..., csp_port: int = ...,
             uart_dev: str = ..., uart_baud: int = ..., can_dev: str = ..., udp_peer_str: str = ..., udp_peer_idx: int = ...,
             tun_conf_str: str = ..., eth_ifname: str = ..., csp_zmqhub_addr: str = ..., csp_zmqhub_idx: int = ..., quiet: int = ...,
             *, module_name: str = None) -> _libparam_typehint:
    """
    Imports and customizes a new instance of the libparam bindings module based on the provided arguments.

    :param csp_address: Which CSP address to use in the module.
    :param csp_version: Which CSP version to use in the module.
    :param csp_hostname: Which CSP hostname to use in the module.
    :param csp_model: Which CSP model to use in the module.
    :param csp_port: Which CSP port to use in the module.
    :param module_name: Optional alternative name of the module to import.
    :param can_dev: Can interface to use.
    :param quiet: Send output from C to /dev/null.
    :return: An instance of the libparam binded module on which operations may be performed.
    """

    init_dict = {key: value for key, value in
                 {
                     'csp_address': csp_address,
                     'csp_version': csp_version,
                     'csp_hostname': csp_hostname,
                     'csp_model': csp_model,
                     'csp_port': csp_port,
                     'uart_dev': uart_dev,
                     'uart_baud': uart_baud,
                     'can_dev': can_dev,
                     'udp_peer_str': udp_peer_str,
                     'udp_peer_idx': udp_peer_idx,
                     'tun_conf_str': tun_conf_str,
                     'eth_ifname': eth_ifname,
                     'csp_zmqhub_addr': csp_zmqhub_addr,
                     'csp_zmqhub_idx': csp_zmqhub_idx,
                     'quiet': quiet,
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


@_contextmanager
def temp_autosend_value(value: int = 0, module: _libparam_typehint = libparam_py3) -> _libparam_typehint:
    """
    Temporarily sets autosend to the provided value for the duration of the context manager block,
    to (for example) ensure that assignments are queued, and retrievals use cached values.

    :param value: The desired value of autosend, during the with block.
    :param module: Libparam bindings module instance to use.
    :return: The specified module.
    """
    original_autosend = module.autosend()
    try:
        module.autosend(value)
        yield module
    finally:
        module.autosend(original_autosend)


# class QueuedActions:
#
#     module: _libparam_typehint = None
#     original_autosend: int = None
#
#     def __init__(self, module: _libparam_typehint = libparam_py3) -> None:
#         self.module = module
#         self.original_autosend = module.autosend()
#         super().__init__()
#
#     def __enter__(self):
#         self.module.autosend(0)
#         return self
#
#     def __exit__(self, exc_type, exc_val, exc_tb):
#         self.module.autosend(self.original_autosend)
