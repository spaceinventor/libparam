#!/usr/bin/python3
"""
Contains and or runs a checkout of a PCDU-P3.
Uses the Libparam Python bindings to perform the checkout.

Please note that the 'PYTHONPATH' and 'LD_LIBRARY_PATH' environment variables must be
set to point at the directory of the python bindings shared object before they may be imported.

To run this script from a CSH directory:
    - cd into the CSH directory.
    - Run the script (with the proper environment variables) using:
    "PYTHONPATH=builddir/lib/param LD_LIBRARY_PATH=builddir/lib/param python3 lib/param/bindings/python/pcdu-p3_checkout.py"
"""

import argparse
from pyparam_utils import Bindings


def get_options() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-d', '--desired_node', default=3, type=int,
                        help="The node to which the PCDU-P3 should be moved to. Defaults to 3.")
    parser.add_argument('-c', '--current_node', default=0, type=int,
                        help="The node on which the PCDU-P3 is located. Defaults to 0.")
    return parser.parse_args()


def _check_boot_cur(bindings, default_node: int):

    "Preparations"

    bindings.ident(default_node)
    # TODO Kevin: Check that the return value of .ident() is a PCDU-P3.
    if input("Does the identity of the current node match the PCDU-P3? [Y/n] (default=Y)").lower() not in {'', 'y'}:
        raise SystemExit("Default node does not match PCDU-P3.")

    bindings.get("boot_cur")

    bindings.reboot(default_node)


def pcdu_p3_checkout(desired_node: int = None, current_node: int = None):
    """
    Attempts to run through a PCDU-P3 checkout on the specified node.
    Does not attempt recovery from exceptions.
    """

    bindings = Bindings()

    default_node = bindings.node(current_node) if current_node is not None else bindings.node()

    if bindings.ping(default_node) < 0:
        raise ConnectionError("Failed to ping current node.")

    if desired_node is not None and desired_node != default_node:
        if bindings.ping(desired_node) >= 0:
            bindings.ident(desired_node)
            raise SystemExit("The desired node is already populated.")

    bindings.list_download(default_node)
    bindings.pull(default_node)

    for _ in range(4):
        _check_boot_cur(bindings, default_node)

    bindings.set("csp_node", desired_node)

    if not desired_node or desired_node == default_node:
        print("Skipping vmem operations...")
    else:
        print(bindings.vmem_list(default_node))
        # TODO Kevin: This confirmation could likely be automated away, if I knew what to look for.
        if input("Please confirm that the vmem list is as desired. [Y/n] (default=Y)").lower() not in {'', 'y'}:
            raise SystemExit("Vmem list mismatch.")

        bindings.vmem_unlock(default_node)
        bindings.vmem_backup(desired_node, default_node)  # TODO Kevin: The script says to use 2, dunno why.
        bindings.reboot(default_node)  # TODO Kevin: The script says to set the gndwdt low and wait, which is likely the same as reboot.

        if bindings.ping(desired_node) < 0:
            raise ConnectionError("The PCDU-P3 does not respond on its new node.")

    # TODO Kevin: Does it make sense to include any of the following in the script?
    new_node = bindings.node()
    bindings.list_download(new_node)
    bindings.pull(new_node)


if __name__ == '__main__':
    pcdu_p3_checkout(get_options().current_node)
