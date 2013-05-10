
Messenger Addressing and Routing
=================================================


Addressing
-------------------------

An address has the following form:

 [ amqp[s]:// ] [user[:password]@] domain [/[name]]

Where domain can be one of:

 host | host:port | ip | ip:port | name

The following are valid examples of addresses:

    * example.org
    * example.org:1234
    * amqp://example.org
    * amqps://example.org
    * example.org/incoming
    * amqps://example.org/outgoing
    * amqps://fred:trustno1@example.org
    * 127.0.0.1:1234
    * amqps://127.0.0.1:1234

The "/name" part of the address, that optionally follows
the domain, is not used by the Messenger library.
For example, if a receiver subscribes to 
    
        amqp://~0.0.0.0:5678

Then it will receive messages sent to

        amqp://~0.0.0.0:5678
as well as
        amqp://~0.0.0.0:5678/foo


Likewise, if the receiver subscribes to

        amqp://~0.0.0.0:5678/foo

it will receive messages addressed to

        amqp://~0.0.0.0:5678/foo
        amqp://~0.0.0.0:5678

and

        amqp://~0.0.0.0:5678/bar




<br/>

Routing
------------------------------

### Pattern Matching, Address Translation, and Message Routing ###

The Messenger library provides message routing capability
with an address translation table.  Each entry in the table 
consists of a *pattern* and a *translation*.

You store a new route entry in the table with the call:

        pn_messenger_route(messenger, pattern, translation);


The address of each outgoing message is compared to the 
table's patterns until the first matching pattern is found,
or until all patterns have failed to match.

If no pattern matches, then Messenger will send (or attempt
to send) your message with the address as given.

If a pattern does match your outgoing message's address, then
Messenger will create a temporary address by transforming
your message's address.  Your message will be sent to the 
transformed address, but **(note!)** the address on your 
outgoing message will not be changed.  The receiver will see 
the original, not the transformed address.


<br/>

### Two Translation Mechanisms ###


Messenger uses two mechanisms to translate addresses.
The first is simple string substitution.


        pattern:     COLOSSUS
        translation: amqp://0.0.0.0:6666
        input addr:  COLOSSUS
        result:      amqp://0.0.0.0:6666


The second mechanism is wildcard/variable substitution.
A wildcard in the pattern matches all or part of your 
input address.  The part of your input address that matched
the wildcard is stored.  The matched value is then inserted
into your translated address in place of numbered variables:
$1, $2, and so on, up to a maximum of 64.

There are two wildcards: * and % .
The rules for matching are:

        * matches anything
        % matches anything but a /
        other characters match themselves
        the whole input addr must be matched


Examples of wildcard matching:

        pattern:      /%/%/%
        translation:  $1x$2x$3
        input addr:   /foo/bar/baz
        result:       fooxbarxbaz

        pattern:      *
        translation:  $1
        inout addr:   /foo/bar/baz
        result:       /foo/bar/baz

        pattern:      /*
        translation:  $1
        input addr:   /foo/bar/baz
        result:       foo/bar/baz

        pattern:      /*baz
        translation:  $1
        input addr:   /foo/bar/baz
        result:       foo/bar/

        pattern:      /%baz
        translation:  $1
        input addr:   /foo/bar/baz
        result:       FAIL

        pattern:      /%/baz
        translation:  $1
        input addr:   /foo/bar/baz
        result:       FAIL

        pattern:      /%/%/baz
        translation:  $1
        input addr:   /foo/bar/baz
        result:       foo

        pattern:      /*/baz
        translation:  $1
        input addr:   /foo/bar/baz
        result:       foo/bar


Examples of route translation usage:

        pattern:     foo
        translation: amqp://foo.com
        explanation: Any message sent to "foo" will be routed to "amqp://foo.com"


        pattern:     bar/*
        translation: amqp://bar.com/$1
        explanation: Any message sent to bar/<path> will be routed to the corresponding path within the amqp://bar.com domain.


        pattern:     amqp:*
        translation: amqps:$1
        explanation: Route all messages over TLS.


        pattern:     amqp://foo.com/*
        translation: amqp://user:password@foo.com/$1
        explanation: Supply credentials for foo.com.


        pattern:     amqp://*
        translation: amqp://user:password@$1
        explanation: Supply credentials for all domains.


        pattern:     amqp://%/*
        translation: amqp://user:password@proxy/$1/$2
        explanation: Route all addresses through a single proxy while preserving the
         original destination.


        pattern:     *
        translation: amqp://user:password@broker/$1
        explanation: Route any address through a single broker.



<br/>

### First Match Wins ###

If you create multiple routing rules, each new rule is appended
to your Messenger's list.  At send-time, Messenger looks down the 
list, and the first rule that matches your outgoing message's 
address wins.  Thus, when creating your routing rules, you should
create them in order of most specific first, most general last.
