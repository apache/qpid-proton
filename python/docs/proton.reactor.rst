#########################
Module ``proton.reactor``
#########################

.. currentmodule:: proton.reactor

Module Summary
##############

|

+---------------------------+----------------------------------------------------------------------------------------------------+
| :class:`Container`        | A representation of the AMQP concept of a ‘container’, which loosely speaking is something that    |
|                           | establishes links to or from another container, over which messages are transfered.                |
+---------------------------+----------------------------------------------------------------------------------------------------+
| :class:`ApplicationEvent` | Application defined event, which can optionally be associated with an engine object and or an      |
|                           | arbitrary subject.                                                                                 |
+---------------------------+----------------------------------------------------------------------------------------------------+
| :class:`EventInjector`    | Can be added to a :class:`Container` to allow events to be triggered by an external thread but     |
|                           | handled on the event thread associated with the container.                                         |
+---------------------------+----------------------------------------------------------------------------------------------------+
| :class:`Backoff`          | A reconnect strategy involving an increasing delay between retries, up to a maximum or 10 seconds. |
+---------------------------+----------------------------------------------------------------------------------------------------+
| :class:`Transaction`      | Tracks the state of an AMQP 1.0 local transaction.                                                 |
+---------------------------+----------------------------------------------------------------------------------------------------+

|

Link Options
============

|

The methods :meth:`Container.create_receiver` and :meth:`Container.create_sender` take one or more link options to allow the details of the links to be customized.

+--------------------------------+----------------------------------------------------------------------------------+
| :class:`LinkOption`            | Abstract interface for link configuration options.                               |
+--------------------------------+----------------------------------------------------------------------------------+
| :class:`ReceiverOption`        | Abstract class for receiver options.                                             |
+--------------------------------+----------------------------------------------------------------------------------+
| :class:`SenderOption`          | Abstract class for sender options.                                               |
+--------------------------------+----------------------------------------------------------------------------------+
| :class:`AtLeastOnce`           | Set at-least-once delivery semantics for message delivery.                       |
+--------------------------------+----------------------------------------------------------------------------------+
| :class:`AtMostOnce`            | Set at-most-once delivery semantics for message delivery.                        |
+--------------------------------+----------------------------------------------------------------------------------+
| :class:`DynamicNodeProperties` | Allows a map of link properties to be set on a link.                             |
+--------------------------------+----------------------------------------------------------------------------------+
| :class:`Filter`                | Receiver option which allows incoming messages to be filtered.                   |
+--------------------------------+----------------------------------------------------------------------------------+
| :class:`Selector`              | Configures a receiver with a message selector filter.                            |
+--------------------------------+----------------------------------------------------------------------------------+
| :class:`DurableSubscription`   | Receiver option which sets both the configuration and delivery state to durable. |
+--------------------------------+----------------------------------------------------------------------------------+
| :class:`Copy`                  | Receiver option which copies messages to the receiver.                           |
+--------------------------------+----------------------------------------------------------------------------------+
| :class:`Move`                  | Receiver option which moves messages to the receiver (rather than copying).      |
+--------------------------------+----------------------------------------------------------------------------------+

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

