from _proton_core import ffi, lib

from _proton_core.lib import pn_incref, pn_decref, \
    pn_record_def, pn_record_set, PN_PYREF

from ._endpoints import pn_record_get


def pn_py2void(obj):
    handle = ffi.new_handle(obj)
    return handle


def pn_void2py(handle):
    if handle == ffi.NULL or handle is None:
        return None
    return ffi.from_handle(handle)


@ffi.def_extern()
def pn_pytracer(transport, message):
    """This was implemented in C previously, need to use Python so I can dereference handle"""
    pytracer = pn_record_get(lib.pn_transport_attachments(transport), lib.PNI_PYTRACER)
    pytracer(transport, message)
