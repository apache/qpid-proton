from _proton_core import ffi, lib

from _proton_core.lib import PN_ACCEPTED, PN_MODIFIED, PN_RECEIVED, PN_REJECTED, PN_RELEASED, pn_delivery_abort, \
    pn_delivery_aborted, pn_delivery_attachments, pn_delivery_link, pn_delivery_local, pn_delivery_local_state, \
    pn_delivery_partial, pn_delivery_pending, pn_delivery_readable, pn_delivery_remote, pn_delivery_remote_state, \
    pn_delivery_settle, pn_delivery_settled, pn_delivery_tag, pn_delivery_update, pn_delivery_updated, \
    pn_delivery_writable, pn_disposition_annotations, pn_disposition_condition, pn_disposition_data, \
    pn_disposition_get_section_number, pn_disposition_get_section_offset, pn_disposition_is_failed, \
    pn_disposition_is_undeliverable, pn_disposition_set_failed, pn_disposition_set_section_number, \
    pn_disposition_set_section_offset, pn_disposition_set_undeliverable, pn_disposition_type, pn_work_next


def _optional(value):
    if value == ffi.NULL:
        return None
    return value


def pn_event_delivery(event):
    return _optional(lib.pn_event_delivery(event))
