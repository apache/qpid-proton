This directory contains java examples that use the messenger API.
Based on the python examples in  ../py

  Send.java - a simple example of using the messenger API to send messages
  Recv.java - a simple example of using the messenger API to receive messages

Note that depending on the address passed into these scripts, you can
use them in either a peer to peer or a brokered scenario.

For brokered usage:
  java Recv.class amqp://<broker>/<queue>
  java Send.class -a amqp://<broker>/<queue> msg_1 ... msg_n

For peer to peer usage:
  # execute on <host> to receive messages from all local network interfaces
  java Recv.class amqp://~0.0.0.0
  java Send.class -a amqp://<host> msg_1 ... msg_n

Or, use the shell scripts "recv" and "send" to run the java programs:
recv [-v] [-n MAXMESSAGES] [-a ADDRESS] ... [-a ADDRESS]
send [-a ADDRESS] [-s SUBJECT] MESSAGE ... MESSAGE

