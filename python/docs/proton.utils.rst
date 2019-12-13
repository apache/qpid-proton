#######################
Module ``proton.utils``
#######################

.. currentmodule:: proton.utils

Module Summary
##############

|

+------------------------------+-----------------------------------------------------------------------+
| :class:`BlockingConnection`  | A synchronous style connection wrapper.                               |
+------------------------------+-----------------------------------------------------------------------+
| :class:`BlockingSender`      | A synchronous sender wrapper.                                         |
+------------------------------+-----------------------------------------------------------------------+
| :class:`BlockingReceiver`    | A synchronous receiver wrapper.                                       |
+------------------------------+-----------------------------------------------------------------------+
| :class:`SyncRequestResponse` | Implementation of the synchronous request-response (aka RPC) pattern. |
+------------------------------+-----------------------------------------------------------------------+

|

Exceptions
==========

|

+---------------------------+---------------------------------------------------------------------------------------------------+
| :class:`SendException`    | Exception used to indicate an exceptional state/condition on a send request.                      |
+---------------------------+---------------------------------------------------------------------------------------------------+
| :class:`LinkDetached`     | The exception raised when the remote peer unexpectedly closes a link in a blocking context, or    |
|                           | an unexpected link error occurs.                                                                  |
+---------------------------+---------------------------------------------------------------------------------------------------+
| :class:`ConnectionClosed` | The exception raised when the remote peer unexpectedly closes a connection in a blocking context, |
|                           | or an unexpected connection error occurs.                                                         |
+---------------------------+---------------------------------------------------------------------------------------------------+

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
