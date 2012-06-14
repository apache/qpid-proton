This directory contains an example client/server application that uses the Proton library.
The applications implement a simple mailbox server.  Clients "post" and "fetch" messages
to named mailboxes on the server.

Files:

    server - the mailbox server.  This server listens to client requests on a well know
    address.  Clients may request to post a message to a mailbox, or fetch a message from
    a mailbox.  When a message is posted, if the mailbox does not exist it is created and
    the message is stored in it.  Should additional messages arrive for the mailbox, they
    are queued in the order of arrival.  When a mailbox is fetched, the next (oldest)
    message in the mailbox is removed from the mailbox and sent to the client.  If a
    client attempts to fetch from a non-existent mailbox, a zero-length message is
    returned.

    post - a client that sends a message to a mailbox on the server.

    fetch - a client that retrieves a message from a mailbox on the server.

To run the example:

    1) Start the server application.  You may specify the address the server should listen
    on.  The default address is 0.0.0.0:5672.  The server application should be left
    running for the following steps.

    2) Post a message to the server using the 'post' application.  For example, the
    following command would post the message "Hello World" to the mailbox "Mailbox-1" on
    server 0.0.0.0:5672 :

              post -m Mailbox-1 "Hello World"

    use the --help option for additional details.

    3) Fetch a message from the server using the 'fetch' application.  For example, the
    following command would fetch the message sent in the previous step:

              fetch Mailbox-1

    use the --help option for additional details.

    Once you are done running the example, you may stop the server application.
