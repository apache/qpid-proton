##########################
Module ``proton.handlers``
##########################

.. currentmodule:: proton.handlers

Module Summary
##############

|

+-------------------------------------+----------------------------------------------------------------------------------------+
| :class:`MessagingHandler`           | A general purpose handler that makes the proton-c events somewhat simpler to deal with |
|                                     | and/or avoids repetitive tasks for common use cases.                                   |
+-------------------------------------+----------------------------------------------------------------------------------------+
| :class:`TransactionHandler`         | The interface for transaction handlers - ie objects that want to be notified of state  |
|                                     | changes related to a transaction.                                                      |
+-------------------------------------+----------------------------------------------------------------------------------------+
| :class:`TransactionalClientHandler` | An extension to the MessagingHandler for applications using transactions.              |
+-------------------------------------+----------------------------------------------------------------------------------------+

|

Exceptions
==========

|

+------------------+-----------------------------------------------------------+
| :class:`Reject`  | An exception that indicates a message should be rejected. |
+------------------+-----------------------------------------------------------+
| :class:`Release` | An exception that indicates a message should be released. |
+------------------+-----------------------------------------------------------+

|

Module Detail
#############

|

.. autoclass:: proton.handlers.MessagingHandler
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: handlers, add

------------

.. autoclass:: proton.handlers.TransactionHandler
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.handlers.TransactionalClientHandler
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: handlers, add

------------

.. autoclass:: proton.handlers.Reject
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.handlers.Release
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

