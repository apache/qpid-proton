from _proton_core import ffi
from _proton_core import lib
from _proton_core.lib import pn_condition_clear, pn_condition_set_name, pn_condition_set_description, pn_condition_info, \
    pn_condition_is_set, pn_condition_get_name, pn_condition_get_description


def _from_str_to_charp(value):
    if value is None:
        return ffi.NULL
    return value.encode()


def pn_condition_set_name(cond, name):
    return lib.pn_condition_set_name(cond, _from_str_to_charp(name))


def pn_condition_set_description(cond, description):
    return lib.pn_condition_set_description(cond, _from_str_to_charp(description))
