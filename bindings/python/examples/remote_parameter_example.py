""" Use multiprocessing to create a remote host looking for a change on a known parameter. """

from __future__ import annotations

import os
import argparse
from time import sleep
from subprocess import Popen
from multiprocessing import Process

CURRENT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))

# The nodes may be changed (Provided that you change them in the .yaml files as well).
LOCAL_NODE, REMOTE_NODE = 17, 18

PARAMETER_IDENTIFIER: str | int = None  # Must be a RAM parameter.
VMEM_COL = 1

OKBLUE = '\033[94m'
OKCYAN = '\033[96m'
ENDC = '\033[0m'

REMOTE_PREFIX = f"{OKBLUE}[remote listener]{ENDC}"
LOCAL_PREFIX = f"{OKCYAN}[local sender]{ENDC}"

remote_print = lambda string: print(f"{REMOTE_PREFIX}\t{string}")
local_print = lambda string: print(f"{LOCAL_PREFIX}\t\t{string}")


def remote_listener(parameter_identifier: int | str):
    """ Wait for the value of a local parameter to change. """
    import libparam_py3
    libparam_py3._param_init(yamlname=f"{CURRENT_DIRECTORY}/remote.yaml", quiet=True)

    param = libparam_py3.Parameter(parameter_identifier)
    original_value = param.value

    remote_print('Waiting for local parameter to be changed remotely...')

    while param.value == original_value:
        remote_print('Still waiting...')
        sleep(0.2)

    remote_print("We just saw a local parameter be changed remotely.")
    remote_print("Let's wrap up by chaning it back to its original value.")

    param.value = original_value  # Reassign the value to the original one.


def local_sender(parameter_identifier: int | str):
    """ Set the value of a remote parameter. """
    import libparam_py3
    libparam_py3._param_init(yamlname=f"{CURRENT_DIRECTORY}/local.yaml", quiet=True)

    local_print("Waiting a bit, to ensure that the remote has noted the original value...")

    sleep(1)  # Make sure the server has read its own parameter list, before we alter it.

    local_print("Downloading a list of all remote parameters...")
    libparam_py3.list_download(REMOTE_NODE)  # We need to download the remote parameters, so the bindings can see them.
    local_print("Creating Parameter object for remote parameter...")
    remote_param = libparam_py3.Parameter(parameter_identifier, node=REMOTE_NODE)

    local_print("Now lets change the remote value, using the Parameter.")

    # Change the value of the remote parameter.
    # Adapt the way we change the parameter, based on its type.
    if remote_param.type is str:
        if remote_param.type is str:
            remote_param.value = "changed example"
    else:
        if isinstance(remote_param, libparam_py3.ParameterArray):
            remote_param[0] += 1
        else:
            remote_param.value += 1


def main(parameter_identifier: int | str):
    """ Potentially maintains a zmqproxy, while the local and remote sessions finish communicating with each other """

    zmqproxy_location = 'zmqproxy'  # Attempt to use from PATH
    if '/csh/lib/param' in CURRENT_DIRECTORY:  # Attempt to use zmqproxy from a CSH build directory.
        zmqproxy_location = f"{'/'.join(CURRENT_DIRECTORY.split('/')[:-5])}/builddir/zmqproxy"

    # Attempt to maintain a zmqproxy while the subprocesses communicate.
    # It's okay for this to fail when another zmqproxy is already running.
    with Popen(zmqproxy_location) as zmqproxy:

        (listener_process := Process(target=remote_listener, args=(parameter_identifier,))).start()
        (sender_process := Process(target=local_sender, args=(parameter_identifier,))).start()

        listener_process.join()
        sender_process.join()

        zmqproxy.terminate()


if __name__ == '__main__':

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('parameter_identifier', help='Name or ID of the RAM parameter to use during the test.')
    args = parser.parse_args()

    param_identifier: str = args.parameter_identifier

    main(int(param_identifier) if param_identifier.isdigit() else param_identifier)
