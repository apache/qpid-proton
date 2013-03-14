Message Disposition
===============================


Messenger disposition operations allow a receiver to accept or
reject specific messages, or ranges of messages.  Senders can
detect the disposition of their messages.


Message States
---------------------------

Messages have one of four different states:  
  * `PN_STATUS_UNKNOWN`  
  * `PN_STATUS_PENDING`  
  * `PN_STATUS_ACCEPTED`  
  * `PN_STATUS_REJECTED`  

<br/>


Windows and Trackers
----------------------------

<br/>


### receiving ###

To explicitly set or get message dispositions, your messenger
must set a positive size for its incoming window:

        pn_messenger_set_incoming_window ( messenger, N );

You can implicity accept messages by simply letting enough
new messages arrive.  As older messages pass beyond the threshold
of your incoming window size, they will be automatically
accepted.  Thus, if you want to automatically accept all
messages as they arrive, you can set your incoming window
size to 0.

To exercise _explicit_ control over particular messages or ranges
of messages, the receiver can use trackers. The call

        pn_messenger_incoming_tracker ( messenger );

will return a tracker for the message most recently returned
by a call to

        pn_messenger_get ( messenger, message );
With a message that is being tracked, the messenger can accept
(or reject) that individual message:

        pn_messenger_accept ( messenger, tracker, 0 );
        pn_messenger_reject ( messenger, tracker, 0 );

Or it can accept (or reject) the tracked message as well as all older
messages back to the limit of the incoming window:

        pn_messenger_accept ( messenger, tracker, PN_CUMULATIVE );
        pn_messenger_reject ( messenger, tracker, PN_CUMULATIVE );

Once a message is accepted or rejected, its status can no longer
be changed, even if you have a separate tracker associated with it.


<br/>
<br/>
   


### sending ###

A sender can learn how an individual message has been received
if it has a positive outgoing window size:

        pn_messenger_set_outgoing_window ( messenger, N );

and if a tracker has been associated with that message in question.  
This call:

        pn_messenger_outgoing_tracker ( messenger );

will return a tracker for the message most recently given to:

        pn_messenger_put ( messenger, message );

To later find the status of the individual tracked message, you can call:

        pn_messenger_status ( messenger, tracker );

The returned value will be one of

* `PN_STATUS_ACCEPTED`
* `PN_STATUS_REJECTED` , or
* `PN_STATUS_PENDING` - If the receiver has not disposed the message yet.  


If either the sender or the receiver simply declares the message (or range of messages) to
be settled, with one of these calls:

        pn_messenger_settle ( messenger, tracker, 0 );
        pn_messenger_settle ( messenger, tracker, PN_CUMULATIVE );

then the sender will see `PN_STATUS_UNKNOWN` as the status of any
settled messages.

<br/>

_Note_  
If a message is rejected by the receiver, it does not mean that
the message was malformed.  Malformed messages cannot be sent.
Even messages with no content are valid messages.
Rejection by a receiver should be understood as the receiver
saying "I don't want this." or possibly  "I don't want this _yet_." 
dependeing on your application.
The sender could decide to try sending the same message again later, 
or to send the message to another receiver, or to discard it.



