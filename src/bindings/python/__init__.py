
# Import everything from the sipyparam namespace,
# because ideally this __init__.py would just be the .so file.
try:  # using symbols already available (from LD_LIBRARY_PATH or embedded interpreter context)
    from .sipyparam import *
except ImportError as e:  # use bundled libparam symbols
    assert "undefined symbol" in e.msg

    from types import ModuleType as _ModuleType

    def _import_libparam_so(package_dir: str = None, so_filename: str = 'libparam_pip.so') -> _ModuleType:
        """ Import bundled libparam.so, while hiding as many of our importation dependencies as possible """

        import os
        from ctypes import CDLL, RTLD_GLOBAL

        # Get the directory of the current __init__.py file
        if package_dir is None:
            package_dir = os.path.dirname(__file__)

        # Construct the full path to the shared object file
        so_filepath = os.path.join(package_dir, so_filename)

        # Load the shared object file using ctypes, so can expose C symbols.
        # This may be Linux specific.
        return CDLL(so_filepath, mode=RTLD_GLOBAL)

    _import_libparam_so()

    try:  # Try again to import, now with libparam symbols
        from .sipyparam import *
    except ImportError as e:  # Assume this is caused by missing libcsp symbols.
        assert "undefined symbol" in e.msg

        # Let their bindings handle symbol resolution.
        # We don't actually need the bindings themselves, so perhaps we should handle linking the .so file ourselves.
        import libcsp_py3

        # We have resolved all the symbols we can, here goes nothing
        from .sipyparam import *
