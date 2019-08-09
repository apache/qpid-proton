#######################
Module ``proton.utils``
#######################

.. currentmodule:: proton.utils

Module Summary
##############

|

.. autosummary::
    BlockingConnection
    BlockingSender
    BlockingReceiver
    SyncRequestResponse

|

Exceptions
==========

|

.. autosummary::
    SendException
    LinkDetached
    ConnectionClosed

|

Module Detail
#############

|

.. autoclass:: proton.utils.BlockingConnection
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: on_transport_tail_closed, on_transport_head_closed, on_transport_closed

------------

.. autoclass:: proton.utils.BlockingSender
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.utils.BlockingReceiver
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.utils.ConnectionClosed
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args, with_traceback

------------

.. autoclass:: proton.utils.LinkDetached
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: args, with_traceback

------------

.. autoclass:: proton.utils.SendException
   :members:
   :show-inheritance:
   :inherited-members:
   :undoc-members:
   :exclude-members: args, with_traceback

------------

.. autoclass:: proton.utils.SyncRequestResponse
   :members:
   :show-inheritance:
   :inherited-members:
   :undoc-members:
