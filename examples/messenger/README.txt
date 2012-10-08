This directory contains example scripts using the messenger API.

  send.py - a simple example of using the messenger API to send messages
  recv.py - a simple example of using the messenger API to receive messages

Note that depending on the address passed into these scripts, you can
use them in either a peer to peer or a brokered scenario.

For brokered usage:

  recv.py amqp://<broker>/<queue>

  send.py -a amqp://<broker>/<queue> msg_1 ... msg_n

For peer to peer usage:

  # execute on <host> to receive messages from all local network interfaces
  recv.py amqp://~0.0.0.0

  send.py -a amqp://<host> msg_1 ... msg_n
