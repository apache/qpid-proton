**qpid-proton-messenger**
---------------------
Apache qpid proton messenger AMQP 1.0 library
http://qpid.apache.org/proton/
https://git-wip-us.apache.org/repos/asf?p=qpid-proton.git

This library provides JavaScript bindings for Apache qpid proton messenger giving AMQP 1.0 support to Node.js and browsers.

**Important Note - Modern Browser Needed**
The JavaScript binding requires ArrayBuffer/TypedArray and WebSocket support. Both of these are available in most "modern" browser versions. The author has only tried running on FireFox and Chrome, though recent Safari, Opera and IE10+ *should* work too - YMMV. It might be possible to polyfill for older browsers but the author hasn't tried this.

**Important Note - WebSocket Transport!!!**
Before going any further it is really important to realise that the JavaScript bindings to Proton are somewhat different to the bindings for other languages because of the restrictions of the execution environment. 

In particular it is very important to note that the JavaScript bindings by default use a WebSocket transport and not a TCP transport, so whilst it's possible to create Server style applications that clients can connect to (e.g. recv.js and send.js) note that:
JavaScript clients cannot *directly* talk to "normal" AMQP applications such as qpidd or (by default) the Java Broker because they use a standard TCP transport.

This is a slightly irksome issue, but there's no getting away from it because it's a security restriction imposed by the browser environment.

**Full README**
https://git-wip-us.apache.org/repos/asf?p=qpid-proton.git;a=blob;f=proton-c/bindings/javascript/README;h=8bfde56632a22bffce4afe791321f4900c5d38d2;hb=HEAD

**Examples**
The examples in the main Proton repository are the best starting point:
https://git-wip-us.apache.org/repos/asf?p=qpid-proton.git;a=tree;f=examples/messenger/javascript;h=37964f32a6b3d63e802000b0a2a974ed017e4688;hb=HEAD

In practice the examples follow a fairly similar pattern to the Python bindings the most important thing to bear in mind though is that JavaScript is completely asynchronous/non-blocking, which can catch the unwary.

An application follows the following (rough) steps:

(optional) Set the heap size.
It's important to realise that most of the library code is compiled C code and the runtime uses a "virtual heap" to support the underlying malloc/free. This is implemented internally as an ArrayBuffer with a default size of 16777216.

To allocate a larger heap an application must set the PROTON_TOTAL_MEMORY global. In Node.js this would look like (see send.js):

    PROTON_TOTAL_MEMORY = 50000000; // Note no var - it needs to be global.

In a browser it would look like (see send.html):

    <script type="text/javascript">PROTON_TOTAL_MEMORY = 50000000</script>

Load the library and create a message and messenger.
In Node.js this would look like (see send.js):

    var proton = require("qpid-proton-messenger");
    var message = new proton.Message();
    var messenger = new proton.Messenger();

In a browser it would look like (see send.html):

    <script type="text/javascript" src="../../../node_modules/qpid-proton-messenger/lib/proton-messenger.js"></script>
    <script type="text/javascript" >
    var message = new proton.Message();
    var messenger = new proton.Messenger();

Set up event handlers as necessary.

    messenger.on('error', <error callback>);
    messenger.on('work', <work callback>);
    messenger.on('subscription', <subscription callback>);

The work callback is triggered on WebSocket events, so in general you would use this to send and receive messages, for example in recv.js we have:

    var pumpData = function() {
        while (messenger.incoming()) {
            var t = messenger.get(message);
    
        console.log("Address: " + message.getAddress());
        console.log("Subject: " + message.getSubject());
    
        // body is the body as a native JavaScript Object, useful for most real cases.
        //console.log("Content: " + message.body);
    
        // data is the body as a proton.Data Object, used in this case because
        // format() returns exactly the same representation as recv.c
        console.log("Content: " + message.data.format());
    
        messenger.accept(t);
       }
    };
    messenger.on('work', pumpData);


The subscription callback is triggered when the address provided in a call to

    messenger.subscribe(<address>);

Gets resolved. An example of its usage can be found in qpid-config.js which is
a fully functioning and complete port of the python qpid-config tool. It also
illustrates how to do asynchronous request/response based applications.

Aside from the asynchronous aspects the rest of the API is essentially the same
as the Python binding aside from minor things such as camel casing method names etc.

