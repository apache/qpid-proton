## Buffering {#buffering}

AMQP and proton have mechanisms to allow an application to control it's use of memory.

### Outgoing Data

The unit of memory control in AMQP is the *session*.
`pn_session_outgoing_bytes()` tells you the total bytes buffered for all
outgoing deliveries on all sending links belonging to that session.

Each call to `pn_link_send()` adds to the session's outgoing byte total.  Each
time proton writes data to the network it reduces the total.  To control the
memory used by a session, check `pn_session_outgoing_bytes()` before
calling `pn_link_send()`. If it is too high, stop sending until the link
gets a @ref PN_LINK_FLOW event.

The AMQP protocol allows peers to exchange session limits so they can predict
their buffering requirements for incoming data (
`pn_session_set_incoming_capacity()` and
`pn_session_set_outgoing_window()`). Proton will not exceed those limits when
sending to or receiving from the peer. However proton does *not* limit the
amount of data buffered in local memory at the request of the application.  It
is up to the application to ensure it does not request more memory than it wants
proton to use.

#### Priority

Data written on different links can be interleaved with data from any other link
on the same connection when sending to the peer. Proton does not make any formal
guarantee of fairness, and does not enforce any kind of priority when deciding
how to order frames for sending. Using separate links and/or sessions for
high-priority messages means their frames *can* be sent before already-buffered
low-priority frames, but there is no guarantee that they *will*.

If you need to ensure swift delivery of higher-priority messages on the same
connection as lower-priority ones, then you should control the amount of data
buffered by proton, and buffer the backlog of low-priority backlog in your own
application.

There is no point in letting proton buffer more than the outgoing session limits
since that's all it can transmit without peer confirmation. You may want to
buffer less, depending on how you value the trade-off between reducing max
latency for high-priority messages (smaller buffer) and increasing max
throughput under load (bigger buffer).

### Incoming Data

To Be Done... <!-- TODO aconway 2018-05-03:  -->
