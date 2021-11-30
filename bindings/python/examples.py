#!/usr/bin/python3
"""
Contains examples of using the libparam Python bindings and its utilities.
See: 'libparam_py3.pyi' for functions and classes available once the bindings have been imported.

Please note that the 'PYTHONPATH' and 'LD_LIBRARY_PATH' environment variables must be
set to point at the directory of the python bindings shared object before they may be imported.

Compiling and importing the bindings and running these examples:
    - Set 'enable_python3_bindings' to <true> in 'meson_options.txt'.
    - cd into project directory.
    - Compile with meson using: "meson . builddir && ninja -C builddir clean && ninja -C builddir".
    - Run these examples (with the proper environment variables) using:
    "PYTHONPATH=builddir/lib/param LD_LIBRARY_PATH=builddir/lib/param python3 lib/param/bindings/python/examples.py"

The examples start executing at "if __name__ == '__main__'".
"""

from __future__ import annotations

from pyparam_utils import Bindings, temp_autosend_value, ParamMapping


LOCAL_NODE = 1


def int_param_examples(bindings) -> None:
    """
    Contains examples of how to get and set the value of a normal int parameters.

    :param bindings: Return value of 'param_utils.Bindings()'.
    """

    # We assign a variable for the 'Parameter' class for ease of access.
    ParamClass = bindings.Parameter

    # Currently; two methods for retrieving and changing the values of parameters are available.
    # The first is simply using the 'get' and 'set' translations available from 'Satctl', shown below:
    # 'bindings.get()' returns the value of a parameter. The first argument is an identifier of the desired parameter,
    # it'll be the ID when it is an int, or the name when it is a string. The second argument is the host.
    original_col_verbose_value: int = bindings.get(202)  # We change the value later, so store this so we can revert.

    # The second method is an object oriented solution.
    # When we create an instance of ParamClass,
    # we may access most attributes of the parameter it represents on the instance directly.
    # Not unlike 'bindings.get()', the first argument to ParamClass() (the Parameter class constructor)
    # is an identifier of the desired parameter, which follow the same rules as before.
    col_verbose = ParamClass(202)
    print(f"We have a wrapper of {col_verbose.name} on node {col_verbose.node}.")
    print(f"The {col_verbose.name} parameter is of type {col_verbose.type}.")
    # See libparam_py3.pyi for a full list of attributes and methods available on the Parameter class.

    # Let's retrieve the value of the parameter through our new instance.
    original_col_verbose_value_by_class: int = col_verbose.value
    print(f"The original value of {col_verbose.name} is {original_col_verbose_value_by_class}.")

    # Let's check if both methods returned the same value.
    if original_col_verbose_value_by_class == original_col_verbose_value:
        print("Either way of receiving values return the same result.")

    print(f"We changed the value of {col_verbose.name} to {original_col_verbose_value + 3}, using the Satctl translations.")
    bindings.set(202, original_col_verbose_value + 3)

    if col_verbose.value != original_col_verbose_value_by_class:
        print(f"We just changed the value of {col_verbose.name} to {col_verbose.value} through the Satctl set command.")
        print("This was reflected on the instance as well, as its value was the same.")

    # Revert the value, and pretend we were never here.
    col_verbose.value = original_col_verbose_value

    print(f"Don\'t worry; we now reverted the value to: {col_verbose.value}")


def string_param_examples(bindings) -> None:
    """
    Contains examples of how string parameters differ from normal parameters.

    :param bindings: Return value of 'param_utils.Bindings()'.
    """

    # We assign a variable for the 'Parameter' class for ease of access.
    ParamClass = bindings.Parameter

    col_cnfstr = ParamClass(200)  # See 'int_param_examples()' for what this does.

    # Even though string parameters are actually represented by character arrays,
    # it's possible to get and set them using Python strings.
    original_col_cnfstr_value: str = col_cnfstr.value  # Store the initial value such that we can reset it.

    col_cnfstr.value = "Hello World!"  # Change the value of the parameter using a string.

    print(f'The initial value of {col_cnfstr.name} was: "{original_col_cnfstr_value}".')
    print(f'We have now changed it to: "{col_cnfstr.value}".')

    # Let's change the value of a character in the string by index.
    # TODO Kevin: Whoops; this doesn't quite work yet, neither here, nor in Satctl.
    #col_cnfstr[2] = 'J'
    #print(f'We inserted a \'J\' into the string, so the new value is "{col_cnfstr.value}".')

    # String parameters can still be indexed like arrays/lists.
    print(f"The first character in the new string value is: '{col_cnfstr[0]}'")

    # Backwards indexation for string parameters behaves in a manner which is probably unexpected.
    # This is because indexation uses the fixed length of the character array,
    # which likely differs from the length of the string value.
    print(f"Perhaps unexpectedly; the last character in the new string value is: '{col_cnfstr[-1]}'")

    print(f"This is because the length of the parameter is: {len(col_cnfstr)},")
    print(f"while the length of the string value is: {len(col_cnfstr.value)}")

    # Revert the value, and pretend we were never here.
    col_cnfstr.value = original_col_cnfstr_value

    print(f'Don\'t worry; we have now reverted the value to: "{col_cnfstr.value}".')


def parameter_list_examples(bindings) -> None:
    """
    Contains examples of how to retrieve and use lists of parameters.

    :param bindings: Return value of 'param_utils.Bindings()'.
    """

    # Lets get a list of all parameters available to us currently.
    parameter_list = bindings.list()

    print("We just retrieved a list of all available parameters.")
    print("Here are their names: ", end='')
    for param in parameter_list:
        print(f"{param.name}, ", end='')
    print()  # Newline

    downloaded_parameter_list = bindings.list_download(LOCAL_NODE)
    print(f"We just downloaded a list of all local parameters, the length of which is {len(downloaded_parameter_list)}.")
    if parameter_list == downloaded_parameter_list:
        print("But its contents were identical with the list we already had.")

    # Here is an example of how a parameter list may be filtered.
    paramlist = bindings.ParameterList([param for param in parameter_list if param.type is int])
    print("We just made a new parameter list filtered to only contain interger parameters.")
    print(f"The length of this list is {len(paramlist)}")

    # ParameterLists allow for multiple Parameters to be 'pulled' or 'pushed' in a single function call,
    # as opposed to needing to 'pull' or 'push' them one by one.
    # Either of these has no effect when autosend is set to 1, but would be required for values to change otherwise.

    # This pulls the value of all the parameters in the list.
    paramlist.pull(LOCAL_NODE)
    print(f"We just updated the values of {', '.join(param.name for param in paramlist)}")

    paramlist.push(LOCAL_NODE)
    print(f"We just pushed the values of the same list.")
    print("We hadn't made any changes however, so this shouldn't have any effect.")


def misc_param_examples(bindings) -> None:
    """
    Contains examples of some other things you can do with these bindings.

    :param bindings: Return value of 'param_utils.Bindings()'.
    """

    # You can change the value of 'autosend' not much unlike how it would be done in Satctl.
    original_autosend = bindings.autosend()
    bindings.autosend(0)
    print(f"The initial value of autosend was {original_autosend}, we have now set it to {bindings.autosend()}")

    # Perhaps you wish to set the value of 'autosend' for a while, and ensure it is reset afterwards.
    # Lucky you; there is a utility context manager for exactly this purpose!
    with temp_autosend_value(1):
        print(f"We have now changed it back to {bindings.autosend()} for a short while.")
    print(f"And have now reset if to {bindings.autosend()}.")

    # We can 'stage' changes to parameters because autosend is 0.
    new_value = "This value will not be applied."
    bindings.set(200, new_value)
    print(f'We set "{new_value}" as the new value of the parameter by ID 200.')
    print(f'But because autosend is 0, its value is still "{bindings.get(200)}".')

    # We can clear all staged changes using:
    bindings.clear()

    print("We just cleared all staged changes.")

    # We can push all staged changes using:
    bindings.push(LOCAL_NODE)  # The first argument is node, the second is a timeout in ms.

    print("We just pushed all staged changes.")
    print(f'But because we cleared the queue first, the value is still: "{bindings.get(200)}"')

    # You can use the ping from Satctl.
    print(f"Ping recieved response in {bindings.ping(LOCAL_NODE)} ms.")

    # As well as ident.
    print(f"Ident of the pinged node: {bindings.ident(LOCAL_NODE).splitlines()}.")

    # We can get and set the current parameter version too.
    print(f"The current parameter version is {bindings.paramver()}.")

    # To get the type of a parameter from its identifier.
    print(f"Type of parameter by ID 200: {bindings.get_type(200)}")


def param_mapping_example() -> None:
    """
    Contains an examples of an alternative way to retrieve Parameter object.
    which requires specifying neither their name nor ID.
    """

    # We instantiate an instance of the ParamMapping class, through which we can access parameters.
    mapping = ParamMapping()

    # The arguments given to 'mapping.CSP_RTABLE()' are passed along to 'libparam_py3.Parameter()',
    # we don't have to worry about the parameter identifier (ID or name), as that is handled for us.
    csp_rtable = mapping.CSP_RTABLE(node=LOCAL_NODE)

    print(f'We retrieved the parameter named "{csp_rtable.name}" on node {csp_rtable.node} from our mapping.')


def param_vmem_examples(bindings) -> None:
    """
    Examples of commands related to vmem.

    :param bindings: Return value of 'param_utils.Bindings()'.
    """

    vmem_list: str = bindings.vmem_list()  # Returns a string listing vmem parameters. Accepts argument for 'node' and 'timeout'.
    print('We just retrieved a list of all vmem parameters, shown below:')
    print(vmem_list)

    # There also exists bindings for:
    # bindings.vmem_restore()
    # bindings.vmeme_backup()
    # bindings.vmem_unlock()


if __name__ == '__main__':

    # While it is entirely legal for us to import libparam_py3 directly,
    # doing so requires us to call libparam_py3._param_init() ourselves,
    # before most operations are permitted.
    # Using the return value of the 'Bindings()' function from 'pyparam_utils',
    # therefore strips the need for most bootstrapping code.
    bindings = Bindings(quiet=True)

    # The following is equivalent to 'Bindings(quiet=False)':
    # libparam_py3._param_init(quiet=True)

    # Just as when using Satctl, the node command returns the current default node,
    # as well as changing it when an integer argument is provided.
    bindings.node(LOCAL_NODE)  # Local
    print(f"We just set the default node to {bindings.node()}.")

    print()  # Newline

    int_param_examples(bindings)

    print()  # Newline

    string_param_examples(bindings)

    print()  # Newline

    parameter_list_examples(bindings)

    print()  # Newline

    misc_param_examples(bindings)

    print()  # Newline

    param_mapping_example()

    print()  # Newline

    param_vmem_examples(bindings)
