"""
Interface documentation of the libparam Python bindings.

The bindings API largely mimics the shell interface to CSH,
meaning that the same commands (with their respective parameters), are available as functions here.
Wrapper classes for param_t's and a class providing an interface to param_queue_t are also made available.
These provide an object-oriented interface to libparam, but are largely meant for convenience.
"""

from __future__ import annotations

from typing import \
    Any as _Any, \
    Iterable as _Iterable

_param_value_hint = int | float | str
_param_type_hint = _param_value_hint | bytearray


class Parameter:
    """
    Wrapper class for Libparam's parameters.
    Provides an interface to their attributes and values.
    """

    def __init__(self, param_identifier: _param_ident_hint, node: int = None, host: int = None) -> None:
        """
        As stated; this class simply wraps existing parameters,
        and cannot create new ones. It therefore requires an 'identifier'
        to the parameter you wish to retrieve.

        :param param_identifier: an int or string of the parameter ID or name respectively.
        :param node: Node on which the parameter is located.

        :raises ValueError: When no parameter can be found from an otherwise valid identifier.
        """

    @property
    def name(self) -> str:
        """ Returns the name of the wrapped param_t C struct. """

    @property
    def unit(self) -> str:
        """ The unit of the wrapped param_t c struct as a string. """

    @property
    def id(self) -> int:
        """ Returns the id of the wrapped param_t C struct. """

    @property
    def node(self) -> int:
        """ Returns the node of the parameter, as in; the one in the wrapped param_t C struct. """

    @node.setter
    def node(self, value: int) -> None:
        """
        Attempts to set the parameter to a matching one, on the specified node.

        :param value: Node on which the other parameter ought to be located.

        :raises TypeError: When attempting to delete or assign the node to an invalid type.
        :raises ValueError: When a matching parameter cannot be found on the specified node.
        """

    @property
    def host(self) -> int | None:
        """ Returns the host the parameter uses for operations. """

    @host.setter
    def host(self, value: int | None) -> None:
        """ Naively sets the node used when querying parameters. Uses node when None. """

    @property
    def type(self) -> _param_type_hint:
        """ Returns the best Python representation type object of the param_t c struct type. i.e int for uint32. """

    @property
    def value(self) -> int | float:
        """
        Returns the value of the parameter from its specified node in the Python representation of its type.
        Array parameters return a tuple of values, whereas normal parameters return only a single value.
        """

    @value.setter
    def value(self, value: int | float) -> None:
        """
        Sets the value of the parameter.

        :param value: New desired value. Assignments to other parameters, use their value instead, Otherwise uses .__str__().
        """

    @property
    def is_array(self) -> bool:
        """ Returns True or False based on whether the parameter is an array (Strings count as arrays)."""

    @property
    def mask(self) -> int:
        """ Returns the mask of the wrapped param_t C struct. """

    @property
    def timestamp(self) -> int:
        """ Returns the timestamp of the wrapped param_t C struct. """


class ParameterArray(Parameter):
    """
    Subclass of Parameter specifically designed to provide an interface
    to array parameters. In practical terms; the class implements:
        __len__(), __getitem__() and __setitem__().
    The Parameter class will automatically create instances of this class when needed,
    which means that manual instantiation of ParameterArrays is unnecessary.
    """

    def __len__(self) -> int:
        """
        Gets the length of array parameters.

        :raises AttributeError: For non-array type parameters.
        :return: The value of the wrapped param_t->array_size.
        """

    def __getitem__(self, index: int) -> _param_type_hint:
        """
        Get the value of an index in an array parameter.

        :param index: Index on which to get the value. Supports backwards subscription (i.e: -1).
        :raises IndexError: When trying to get a value outside the bounds of the parameter array.
        :raises ConnectionError: When autosend is on, and no response is received.

        :return: The value of the specified index, as its Python type.
        """

    def __setitem__(self, index: int, value: int | float) -> None:
        """
        Set the value of an index in an array parameter.

        :param index: Index on which to set the value. Supports backwards subscription (i.e: -1).
        :param value: New value to set, should match the type of the parameter.
        :raises IndexError: When trying to set value ouside the bounds the parameter array.
        :raises ConnectionError: When autosend is on, and no response is received.
        """

    @property
    def value(self) -> str | tuple[int | float]:
        """
        Returns the value of the parameter from its specified node in the Python representation of its type.
        Array parameters return a tuple of values, whereas normal parameters return only a single value.
        """

    @value.setter
    def value(self, value: str | _Iterable[int | float]) -> None:
        """
        Sets the value of the parameter.

        :param value: New desired value. Assignments to other parameters, use their value instead, Otherwise uses .__str__().
        """


class ParameterList(list[Parameter, ParameterArray]):
    """
    Convenience class providing an interface for pulling and pushing the value of multiple parameters
    in a single request using param_queue_t's
    """

    def __init__(self, *args: Parameter) -> None:
        """ Accepts a sequence of Parameter object as is initial values. """

    def append(self, __object: Parameter) -> None:
        """
        Appends the specified Parameter object to the list.

        :raises TypeError: When attempting to append a non-Parameter object.
        """

    def pull(self, host: int, timeout: int = None) -> None:
        """
        Pulls all Parameters in the list as a single request.

        :raises ConnectionError: When no response is received.
        """

    def push(self, node: int, timeout: int = None) -> None:
        """
        Pushes all Parameters in the list as a single request.

        :raises ConnectionError: When no response is received.
        """


_param_ident_hint = int | str | Parameter  # Types accepted for finding a param_t


# Libparam commands
def get(param_identifier: _param_ident_hint, host: int = None, node: int = None, offset: int = None) -> _param_value_hint | tuple[_param_value_hint]:
    """
    Get the value of a parameter.

    :param param_identifier: string name, int id or Parameter object of the desired parameter.
    :param host: The host from which the value should be retrieved (has priority over node).
    :param node: The node from which the value should be retrived.
    :param offset: Index to use for array parameters.

    :raises TypeError: When an invalid param_identifier type is provided.
    :raises ValueError: When a parameter could not be found.
    :raises RuntimeError: When called before ._param_init().

    :return: The value of the retrieved parameter (As its Python type).
    """

def set(param_identifier: _param_ident_hint, value: _param_value_hint | _Iterable[int | float], host: int = None, node: int = None, offset: int = None) -> None:
    """
    Set the value of a parameter.

    :param param_identifier: string name, int id or Parameter object of the desired parameter.
    :param value: The new value of the parameter. .__str__() of the provided object will be used.
    :param host: The host from which the value should be retrieved (has priority over node).
    :param node: The node from which the value should be retrieved.
    :param offset: Index to use for array parameters.

    :raises TypeError: When an invalid param_identifier type is provided.
    :raises ValueError: When a parameter could not be found.
    :raises RuntimeError: When called before ._param_init().
    """

def push(node: int, timeout: int = None) -> None:
    """
    Push the current queue.

    :param node: Node to which the current queue should be pushed.
    :param timeout: Timeout in milliseconds of the push request.
    :raises ConnectionError: when no response is received.
    """

def pull(host: int, include_mask: str = None, exclude_mask: str = None, timeout: int = None) -> None:
    """ Pull all or a specific set of parameters. """

def clear() -> None:
    """ Clears the queue. """

def node(node: int = None) -> int:
    """
    Used to get or change the default node.

    :param node: Integer to change the default node to.
    :return: The current default node.
    """

def paramver(paramver: int = None) -> int:
    """
    Used to get or change the parameter version.

    :param paramver: Integer to change the parameter version to.
    :return: The current parameter version.
    """

def autosend(autosend: int = None) -> int:
    """
    Used to get or change whether autosend is enabled.

    :param autosend: Integer value (representing True or False) to set autosend to.
    :return: The current value/status of autosend.
    """

def queue() -> None:
    """ Print the current status of the queue. """

def list(mask: str = None) -> ParameterList:
    """
    List all known parameters.

    :param mask: Mask on which to filter the list.
    """

def list_download(node: int, timeout: int = None, version: int = None) -> ParameterList:
    """ Download all parameters on the specified node. """


# Commands from CSP
def ping(node: int, timeout: int = None, size: int = None) -> int:
    """
    Ping the specified node.

    :param node: Address of subsystem.
    :param timeout: Timeout in ms to wait for reply.
    :param size: Payload size in bytes.

    :raises RuntimeError: When called before ._param_init().

    :return: >0 = echo time in mS on success, otherwise -1 for error.
    """

def ident(node: int, timeout: int = None) -> str:
    """
    Print the identity of the specified node.

    :param node: Address of subsystem.
    :param timeout: Timeout in ms to wait for reply.

    :raises RuntimeError: When called before ._param_init().
    :raises ConnectionError: When no response is received.
    """

def reboot(node: int) -> None:
    """ Reboot the specified node. """


def get_type(param_identifier: _param_ident_hint, node: int = None) -> _param_type_hint:
    """
    Gets the type of the specified parameter.

    :param param_identifier: string name, int id or Parameter object of the desired parameter.
    :param node: Node of the parameter.
    :return: The best Python representation type object of the param_t c struct type. i.e int for uint32.
    """


# Vmem commands
def vmem_list(node: int = None, timeout: int = None) -> str:
    """
    Builds a string of the vmem at the specified node.

    :param node: Node from which the vmem should be listed.
    :param timeout: Timeout in ms when connecting to the node.

    :raises RuntimeError: When called before ._param_init().
    :raises ConnectionError: When the timeout is exceeded attempting to connect to the specified node.
    :raises MemoryError: When allocation for a CSP buffer fails.

    :return: The string of the vmem at the specfied node.
    """

def vmem_restore(node: int, vmem_id: int, timeout: int = None) -> int:
    """
    Restore the configuration on the specified node.

    :param node: Node on which the configuration should be restored from vmem.
    :param timeout: Timeout in ms when connecting to the node.

    :raises RuntimeError: When called before ._param_init().
    :raises ConnectionError: When the timeout is exceeded attempting to connect to the specified node.

    :return: Int indicating the result of the operation, 0 for success.
    """

def vmem_backup(node: int, vmem_id: int, timeout: int = None) -> int:
    """
    Back up the configuration on the specified node.

    :param node: Node on which the configuration should be backed up to vmem.
    :param timeout: Timeout in ms when connecting to the node.

    :raises RuntimeError: When called before ._param_init().
    :raises ConnectionError: When the timeout is exceeded attempting to connect to the specified node.

    :return: Int indicating the result of the operation, 0 for success.
    """

def vmem_unlock(node: int = None, timeout: int = None) -> int:
    """
    Unlock the vmem on the specified node, such that it may be changed by a backup (for example).

    :param node: Node on which the vmem should be unlocked.
    :param timeout: Timeout in ms when connecting to the node.

    :raises RuntimeError: When called before ._param_init().
    :raises ConnectionError: When the timeout is exceeded attempting to connect to the specified node.
    :raises MemoryError: When allocation for a CSP buffer fails.

    :return: Int indicating the result of the operation, 0 for success.
    """


def _param_init(csp_version = None, csp_hostname: str = None, csp_model: str = None,
                use_prometheus: int = None, rtable: str = None, yamlname: str = None, dfl_addr: int = None, quiet: int = None) -> None:
    """
    Initializes the module, with the provided settings.

    :param csp_version: Which CSP version to use in the module.
    :param csp_hostname: Which CSP hostname to use in the module.
    :param csp_model: Which CSP model to use in the module.
    """
