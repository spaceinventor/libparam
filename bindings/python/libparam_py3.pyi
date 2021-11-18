""" Interface documentation of the libparam Python bindings. """


from typing import Any as _Any


_param_value_hint = int | float | str
_param_type_hint = _param_value_hint | bytearray


class Parameter:

    def __init__(self, param_identifier: _param_ident_hint, node: int = None) -> None: ...

    @property
    def name(self) -> str: ...

    @property
    def unit(self) -> str: ...

    @property
    def id(self) -> int: ...

    @property
    def node(self) -> int: ...

    @node.setter
    def node(self, value: int) -> None: ...

    @property
    def type(self) -> _param_type_hint: ...

    @property
    def value(self) -> _param_value_hint: ...

    @value.setter
    def value(self, value: str) -> None: ...


_param_ident_hint = int | str | Parameter


def get(param_identifier: _param_ident_hint, host: int = None, node: int = None, offset: int = None) -> _param_value_hint | _Any: ...


def set(param_identifier: _param_ident_hint, strvalue: str, host: int = None, node: int = None, offset: int = None) -> None: ...


def push(node: int, timeout: int = None) -> None: ...


def pull(host: int, include_mask: str = None, exclude_mask: str = None, timeout: int = None) -> None: ...


def clear() -> None: ...


def node(node: int = None) -> int: ...


def paramver(paramver: int = None) -> int: ...


def autosend(autosend: int = None) -> int: ...


def queue() -> None: ...


def list(mask: str) -> None: ...


def list_download(node: int, timeout: int = None, version: int = None) -> None: ...


def ping(node: int, timeout: int = None, size: int = None) -> int: ...


def ident(node: int, timeout: int = None, size: int = None) -> None: ...


def get_type(param_identifier: _param_ident_hint, node: int = None) -> _param_type_hint: ...
