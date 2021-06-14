
from _proton_cffi import fii, lib


@ffi.def_exten()
def pn_void2py(obj):
    return ffi.from_handle(obj)