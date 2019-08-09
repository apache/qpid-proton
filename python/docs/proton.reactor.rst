#########################
Module ``proton.reactor``
#########################

.. currentmodule:: proton.reactor

Module Summary
##############

|

.. autosummary::
    Container
    ApplicationEvent
    EventInjector
    Backoff
    Transaction

|

Link Options
============

|

The methods :meth:`Container.create_receiver` and :meth:`Container.create_sender` take one or more link options to allow the details of the links to be customized.

.. autosummary::
    LinkOption
    ReceiverOption
    SenderOption
    AtLeastOnce
    AtMostOnce
    DynamicNodeProperties
    Filter
    Selector
    DurableSubscription
    Copy
    Move

|

Module Detail
#############

|

.. autoclass:: proton.reactor.ApplicationEvent
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: handler

------------

.. autoclass:: proton.reactor.AtLeastOnce
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: test

------------

.. autoclass:: proton.reactor.AtMostOnce
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: test

------------

.. autoclass:: proton.reactor.Backoff
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.reactor.Container
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: on_error, global_handler, timeout, yield_, mark, now, handler, wakeup, start, quiesced, process, stop, stop_events, timer_tick, timer_deadline, acceptor, connection, connection_to_host, set_connection_host, get_connection_address, update, push_event

------------

.. autoclass:: proton.reactor.Copy
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: test

------------

.. autoclass:: proton.reactor.DurableSubscription
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: test

------------

.. autoclass:: proton.reactor.DynamicNodeProperties
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: test

------------

.. autoclass:: proton.reactor.EventInjector
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.reactor.Filter
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: test

------------

.. autoclass:: proton.reactor.LinkOption
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:

------------

.. autoclass:: proton.reactor.Move
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: test

------------

.. autoclass:: proton.reactor.ReceiverOption
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: test

------------

.. autoclass:: proton.reactor.Selector
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: test

------------

.. autoclass:: proton.reactor.SenderOption
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: test

------------

.. autoclass:: proton.reactor.Transaction
    :members:
    :show-inheritance:
    :inherited-members:
    :undoc-members:
    :exclude-members: declare, discharge, update, handle_outcome

