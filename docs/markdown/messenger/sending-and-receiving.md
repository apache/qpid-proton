Sending and Receiving Messages
=======================================================

The Proton Messenger API provides a mixture of synchronous
and asynchronous operations to give you flexibility in
deciding when you application should block waiting for I/O,
and when it should not.


When sending messages, you can:

* send a message immediately,
* enqueue a message to be sent later,
* block until all enqueued messages are sent,
* send enqueued messages until a timeout occurs, or
* send all messages that can be sent without blocking.

When receiving messages, you can:

* receive messages that can be received without blocking,
* block until at least one message is received,
* receive no more than a fixed number of messages.



Functions
------------------------------

* `pn_messenger_put(messenger)`

    Stage message for later transmission, and possibly
    transmit any messages currently staged that are not
    blocked.
    This function will not block.



* `pn_messenger_send(messenger)`

    If messenger timeout is negative (initial default),
    block until all staged messages have been sent.

    If messenger timeout is 0, send all messages that
    can be sent without blocking.

    If messenger timeout is positive, send all messages
    that can be sent until timeout expires.

    *note: If there are any messages that can be received
    when `pn_messenger_send()` is called, they will
    be received.*



* `pn_messenger_get(messenger, msg)`

    Dequeue the head of the incoming message queue to
    your application.
    This call does not block.



* `pn_messenger_recv(messenger)`

    If messenger timeout is negative(initial default),
    block until at least one message is received.

    If timeout is 0, receive whatever messages are available,
    but do not block.

    If timeout is positive, receive available messages until
    timeout expires.

    *note: If there are any unblocked outgoing messages,
    they will be sent during this call.*





Examples
------------------------------

* send a message immediately

        pn_messenger_put(messenger, msg);
        pn_messenger_send(messenger);



* enqueue a message to be sent later

        pn_messenger_put(messenger, msg);

    *note:
    The message will be sent whenever it is not blocked and
    the Messenger code has other I/O work to be done.*



* block until all enqueued messages are sent

        pn_messenger_set_timeout(messenger, -1);
        pn_messenger_send(messenger);

    *note:
    A negative timeout means 'forever'.  That is the initial
    default for a messenger.*



* send enqueued messages until a timeout occurs

        pn_messenger_set_timeout(messenger, 100); /* 100 msec */
        pn_messenger_send(messenger);



* send all messages that can be sent without blocking

        pn_messenger_set_timeout(messenger, 0);
        pn_messenger_send(messenger);



* receive messages that can be received without blocking

        pn_messenger_set_timeout(messenger, 0);
        pn_messenger_recv(messenger, -1);


* block until at least one message is received

        pn_messenger_set_timeout(messenger, -1);
        pn_messenger_recv(messenger, -1);

    *note: -1 is initial messenger default.*



* receive no more than a fixed number of messages

        pn_messenger_recv(messenger, 10);

