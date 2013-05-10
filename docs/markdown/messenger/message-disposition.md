Message Disposition
===============================


Messenger disposition operations allow a receiver to accept or
reject specific messages, or ranges of messages.  Senders can
then detect the disposition of their messages.


Message States
---------------------------

Messages have one of four different states:  
        `PN_STATUS_UNKNOWN`  
        `PN_STATUS_PENDING`  
        `PN_STATUS_ACCEPTED`  
        `PN_STATUS_REJECTED`  

<br/>


Windows and Trackers
----------------------------

<br/>

Messenger does not track the disposition of every message that
it sends or receives.  To set (or get) the disposition of a 
message, that message must be within your incoming (or outgoing)
window.

( I will use the incoming direction as the example.  The outgoing
direction works similarly. )

When you call
  
        pn_messenger_set_incoming_window(messenger, window_size);

you have only declared the window size.  The window is not yet
created.  The window will be created when you issue your first
call to 

        pn_messenger_get(messenger, msg);

And the window will be further populated only by further calls to
pn_messenger_get().







### Receiving ###

To explicitly set or get message dispositions, your messenger
must set a positive size for its incoming window:

        pn_messenger_set_incoming_window(messenger, N);

You can implicity accept messages by simply letting enough
new messages arrive.  As older messages pass beyond the threshold
of your incoming window size, they will be automatically
accepted.  Thus, if you want to automatically accept all
messages as they arrive, you can set your incoming window
size to 0.

To exercise *explicit* control over particular messages or ranges
of messages, the receiver can use trackers. The call

        pn_messenger_incoming_tracker(messenger);

will return a tracker for the message most recently returned
by a call to

        pn_messenger_get(messenger, message);
With a message that is being tracked, the messenger can accept
(or reject) that individual message:

        pn_messenger_accept(messenger, tracker, 0);
        pn_messenger_reject(messenger, tracker, 0);

Or it can accept (or reject) the tracked message as well as all older
messages back to the limit of the incoming window:

        pn_messenger_accept(messenger, tracker, PN_CUMULATIVE);
        pn_messenger_reject(messenger, tracker, PN_CUMULATIVE);

Once a message is accepted or rejected, its status can no longer
be changed, even if you have a separate tracker associated with it.



<br/>

###When to Accept###

Although you *can* accept messages implicitly by letting them fall 
off the edge of your incoming window, you *shouldn't*.  Message
disposition is an important form of communication to the sender.
The best practice is to let the sender know your response, by 
explicit acceptance or rejection, as soon as you can.  Implicitly 
accepting messages by allowing them to fall off the edge of the 
incoming window could delay your response to the sender for an 
unpredictable amount of time.

A nonzero window size places a limit on
how much state your Messenger needs to track.

<br/>

###Accepting by Accident####

If you allow a message to "fall off the edge" of your incoming 
window before you have explicitly accepted or rejected it, then
it will be accepted automatically.

But since your incoming window is only filled by calls to 

        pn_messenger_get(messenger, msg);

messages cannot be forced to fall over the edge by simply 
receiving more messages.  Messages will not be forced over the
edge of the incoming window unless you make too many calls to
`pn_messenger_get()` without explicitly accepting or rejecting 
the messages.

Your application should accept or reject each message as soon 
as practical after getting and processing it.




<br/>
<br/>
   


### Sending ###

A sender can learn how an individual message has been received
if it has a positive outgoing window size:

        pn_messenger_set_outgoing_window(messenger, N);

and if a tracker has been associated with that message in question.  
This call:

        pn_messenger_outgoing_tracker(messenger);

will return a tracker for the message most recently given to:

        pn_messenger_put(messenger, message);

To later find the status of the individual tracked message, you can call:

        pn_messenger_status(messenger, tracker);

The returned value will be one of

* `PN_STATUS_ACCEPTED`
* `PN_STATUS_REJECTED` , or
* `PN_STATUS_PENDING` - If the receiver has not disposed the message yet.  


If either the sender or the receiver simply declares the message (or range of messages) to
be settled, with one of these calls:

        pn_messenger_settle(messenger, tracker, 0);
        pn_messenger_settle(messenger, tracker, PN_CUMULATIVE);

then the sender will see `PN_STATUS_PENDING` as the status of any
settled messages.

<br/>


### Message Rejection ###
If a message is rejected by the receiver, it does not mean that
the message was malformed.  Malformed messages cannot be sent.
Even messages with no content are valid messages.
Rejection by a receiver should be understood as the receiver
saying "I don't want this." or possibly  "I don't want this *yet*." 
depending on your application.
The sender could decide to try sending the same message again later, 
or to send the message to another receiver, or to discard it.

The AMQP 1.0 specification permits a distinction
between *rejecting* the message, and *releasing* the message,
but the Proton library does not expose the *releasing* 
disposition.





