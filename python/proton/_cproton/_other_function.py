from _proton_core import ffi, lib

from _proton_core.lib import pn_incref, pn_decref, \
    pn_record_get, pn_record_def, pn_record_set, \
    PN_PYREF

def pn_py2void(obj):
    return ffi.new_handle(obj)


def pn_void2py(handle):
    if handle == ffi.NULL or handle == None:
        return None
    return ffi.from_handle(handle)

