Most (though not all) of the current examples require a broker or
similar intermediary that supports the AMQP 1.0 protocol, allows
anonymous connections and accepts links to and from a node named
'examples'. A very simple broker emulating script - broker.py - is
provided against which the examples can also be run (transactions are
not yet supported in this script).

Note: For builds that include SASL support via the cyrus sasl library,
those examples that accept incoming connections may require some SASL
configuration which is described below.

------------------------------------------------------------------

helloworld.py

Basic example that connects to an intermediary on localhost:5672,
establishes a subscription from the 'examples' node on that
intermediary, then creates a sending link to the same node and sends
one message. On receiving the message back via the subscription, the
connection is closed.

helloworld_blocking.py

The same as the basic helloworld.py, but using a
synchronous/sequential style wrapper on top of the
asynchronous/reactive API. The purpose of this example is just to show
how different functionality can be easily layered should it be
desired.

helloworld_direct.py

A variant of the basic helloworld example, that does not use an
intermediary, but listens for incoming connections itself. It
establishes a connection to itself with a link over which a single
message is sent. This demonstrates the ease with which a simple daemon
can be built using the API.

helloworld_tornado.py
helloworld_direct_tornado.py

These are variant of the helloworld.py and helloworld_direct.py
examples that use the event loop from the tornado library, rather than
that provided within proton itself and demonstrate how proton can be
used with external loops.

-------------------------------------------------------------------

simple_send.py

An example of sending a fixed number of messages and tracking their
(asynchronous) acknowledgement. Handles disconnection while
maintaining an at-least-once guarantee (there may be duplicates, but
no message in the sequence should be lost). Messages are sent through
the 'examples' node on an intermediary accessible on port 5672 on
localhost.

simple_recv.py

Subscribes to the 'examples' node on an intermediary accessible on port 5672 on
localhost. Simply prints out the body of received messages.

db_send.py

A more realistic sending example, where the messages come from records
in a simple database table. On being acknowledged the records can be
deleted from the table. The database access is done in a separate
thread, so as not to block the event thread during data
access. Messages are sent through the 'examples' node on an
intermediary accessible on port 5672 on localhost.

db_recv.py

A receiving example that records messages received from the 'examples'
node on localhost:5672 in a database table and only acknowledges them
when the insert completes. Database access is again done in a separate
thread from the event loop.

db_ctrl.py

A utility for setting up the database tables for the two examples
above. Takes two arguments, the action to perform and the name of the
database on which to perform it. The database used by db_send.py is
src_db, that by db_recv.py is dst_db. The valid actions are 'init',
which creates the table, 'list' which displays the contents and
'insert' which inserts records from standard-in and is used to
populate src_db, e.g. for i in `seq 1 50`; do echo "Message-$i"; done
| ./db_ctrl.py insert src_db.

tx_send.py

A sender that sends messages in atomic batches using local
transactions (this example does not persist the messages in any way).

tx_recv.py

A receiver example that accepts batches of messages using local
transactions.

tx_recv_interactive.py

A testing utility that allow interactive control of the
transactions. Actions are keyed in to the console, 'fetch' will
request another message, 'abort' will abort the transaction, 'commit'
will commit it.

The various send/recv examples can be mixed and matched if desired.

direct_send.py

An example that accepts incoming links and connections over which it
will then send out messages. Can be used with simple_recv.py or
db_recv.py for direct, non-intermediated communication.

direct_recv.py

An example that accepts incoming links and connections over which it
will then receive messages, printing out the content. Can be used with
simple_send.py or db_send.py for direct, non-intermediated
communication.

-------------------------------------------------------------------

client.py

The client part of a request-response example. Sends requests and
prints out responses. Requires an intermediary that supports the AMQP
1.0 dynamic nodes on which the responses are received. The requests
are sent through the 'examples' node.

server.py

The server part of a request-response example, that receives requests
via the examples node, converts the body to uppercase and sends the
result back to the indicated reply address.

sync_client.py

A variant of the client part, that uses a blocking/synchronous style
instead of the reactive/asynchronous style.

client_http.py

A variant of the client part that takes the input to be submitted in
the request over HTTP (point your browser to localhost:8888/client)

server_tx.py

A variant of the server part that consumes the request and sends out
the response atomically in a local transaction.

direct_server.py

A variant of the server part of a request-response example, that
accepts incoming connections and does not need an intermediary. Much
like the original server, it receives incoming requests, converts the
body to uppercase and sends the result back to the indicated reply
address. Can be used in conjunction with any of the client
alternatives.

-------------------------------------------------------------------

selected_recv.py

An example that uses a selector filter.

-------------------------------------------------------------------

recurring_timer.py

An example showing a simple timer event.

recurring_timer_tornado.py

A variant of the above that uses the tornado eventloop instead.

-------------------------------------------------------------------

SASL configuration

If your build includes extra SASL support (provided via the cyrus SASL
library), you may need to provide some configuration to enable
examples that accept incoming connections (i.e. those with 'direct' in
the name). This is done by supplying a config file name
proton-server.conf. The directory in which it is in can be specified
via the PN_SASL_CONFIG_PATH environment variable. A simple example
config file is included along with these examples, enabling only the
EXTERNAL and ANONYMOUS mechanisms by default.