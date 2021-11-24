""" Interface documentation of the libparam Python bindings. """


from typing import Any as _Any


_param_value_hint = int | float | str
_param_type_hint = _param_value_hint | bytearray


class Parameter:

    def __init__(self, param_identifier: _param_ident_hint, node: int = None) -> None: ...

    @property
    def name(self) -> str:
        """ Returns the name of the wrapped param_t c struct. """

    @property
    def unit(self) -> str:
        """ The unit of the wrapped param_t c struct as a string. """

    @property
    def id(self) -> int:
        """ Returns the id of the wrapped param_t c struct. """

    @property
    def node(self) -> int:
        """ Returns a node integer stored in the wrapper class. This node has priority over the one in the parameter itself. """

    @node.setter
    def node(self, value: int) -> None:
        """ Sets the node stored in the wrapper class. """

    @property
    def type(self) -> _param_type_hint:
        """ Returns the best Python representation type object of the param_t c struct type. i.e int for uint32. """

    @property
    def value(self) -> _param_value_hint:
        """ Returns the cached value of the parameter from the specified node in the Python representation of its type """

    @value.setter
    def value(self, value: str) -> None:
        """
        Sets the value of the parameter.

        :param value: New desired value. .__str__() of the provided object will be used.
        """

class ParameterList(list[Parameter]):

    def __init__(self, *args: Parameter) -> None:
        """ Accepts a sequence of Parameter object as is initial values. """

    def append(self, __object: Parameter) -> None:
        """
        Appends the specified Parameter object to the list.

        :raises TypeError: When attempting to append a non-Parameter object.
        """

    def pull(self) -> None:
        """
        Pulls all Parameters in the list as a single request.

        :raises ConnectionError: When no response is received.
        """

    def push(self, node: int, timeout: int = None) -> None:
        """
        Pushes all Parameters in the list as a single request.

        :raises ConnectionError: When no response is received.
        """


_param_ident_hint = int | str | Parameter


def get(param_identifier: _param_ident_hint, host: int = None, node: int = None, offset: int = None) -> _param_value_hint | _Any:
    """
    Get the value of a parameter.

    :param param_identifier: string name, int id or Parameter object of the desired parameter.
    :param host: The host from which the value should be retrieved (has priority over node).
    :param node: The node from which the value should be retrived.

    :raises TypeError: When an invalid param_identifier type is provided.
    :raises ValueError: When a parameter could not be found.
    :raises RuntimeError: When run before ._param_init() has been called.

    :return: The value of the retrieved parameter (As its Python type).
    """

def set(param_identifier: _param_ident_hint, value: str, host: int = None, node: int = None, offset: int = None) -> None:
    """
    Set the value of a parameter.

    :param param_identifier: string name, int id or Parameter object of the desired parameter.
    :param value: The new value of the parameter. .__str__() of the provided object will be used.
    :param host: The host from which the value should be retrieved (has priority over node).
    :param node: The node from which the value should be retrived.

    :raises TypeError: When an invalid param_identifier type is provided.
    :raises ValueError: When a parameter could not be found.
    :raises RuntimeError: When run before ._param_init() has been called.
    """

def push(node: int, timeout: int = None) -> None:
    """
    Push the current queue

    :param timeout: Timeout in milliseconds of the push request.
    :raises ConnectionError: when no response is received.
    """

def pull(host: int, include_mask: str = None, exclude_mask: str = None, timeout: int = None) -> None: ...

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

def list(mask: str) -> ParameterList:
    """
    List all known parameters.

    :param mask: Mask on which to filter the list.
    """

def list_download(node: int, timeout: int = None, version: int = None) -> ParameterList: ...

def ping(node: int, timeout: int = None, size: int = None) -> int:
    """
    Ping the specified node.

    :param node: Address of subsystem.
    :param timeout: Timeout in ms to wait for reply.
    :param size: Payload size in bytes.

    :raises RuntimeError: When run before ._param_init() has been called.

    :return: >0 = echo time in mS on success, otherwise -1 for error.
    """

def ident(node: int, timeout: int = None) -> None:
    """
    Print the identity of the specified node.

    :param node: Address of subsystem.
    :param timeout: Timeout in ms to wait for reply.

    :raises RuntimeError: When run before ._param_init() has been called.
    :raises ConnectionError: When no response is received.
    """

def get_type(param_identifier: _param_ident_hint, node: int = None) -> _param_type_hint:
    """
    Gets the type of the specified parameter.

    :param param_identifier: string name, int id or Parameter object of the desired parameter.
    :param node: Node of the parameter.
    :return: The best Python representation type object of the param_t c struct type. i.e int for uint32.
    """

def _param_init(csp_address: int = None, csp_version = None, csp_hostname: str = None, csp_model: str = None,
                csp_port: int = None, uart_dev: str = None, uart_baud: int = None, can_dev: str = None, udp_peer_str: str = None, udp_peer_idx: int = None,
                tun_conf_str: str = None, eth_ifname: str = None, csp_zmqhub_addr: str = None,
                csp_zmqhub_idx: int = None, quiet: int = None) -> None:
    """
    Initializes the libparam shared object module, with the provided settings.

    :param csp_address: Which CSP address to use in the module.
    :param csp_version: Which CSP version to use in the module.
    :param csp_hostname: Which CSP hostname to use in the module.
    :param csp_model: Which CSP model to use in the module.
    :param csp_port: Which CSP port to use in the module.
    :param can_dev: Can interface to use.
    """
