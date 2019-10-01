package electron_test

import (
	"fmt"
	"log"
	"net"
	"github.com/apache/qpid-proton/go/pkg/amqp"
	"github.com/apache/qpid-proton/go/pkg/electron"
	"sync"
)

// Example Server that accepts a single Connection, Session and Receiver link
// and prints messages received until the link closes.
func Server(l net.Listener) {
	cont := electron.NewContainer("server")
	c, err := cont.Accept(l)
	if err != nil {
		log.Fatal(err)
	}
	l.Close() // This server only accepts one connection
	// Process incoming endpoints till we get a Receiver link
	var r electron.Receiver
	for r == nil {
		in := <-c.Incoming()
		switch in := in.(type) {
		case *electron.IncomingSession, *electron.IncomingConnection:
			in.Accept() // Accept the incoming connection and session for the receiver
		case *electron.IncomingReceiver:
			in.SetCapacity(10)
			in.SetPrefetch(true) // Automatic flow control for a buffer of 10 messages.
			r = in.Accept().(electron.Receiver)
		case nil:
			return // Connection is closed
		default:
			in.Reject(amqp.Errorf("example-server", "unexpected endpoint %v", in))
		}
	}
	go func() { // Reject any further incoming endpoints
		for in := range c.Incoming() {
			in.Reject(amqp.Errorf("example-server", "unexpected endpoint %v", in))
		}
	}()
	// Receive messages till the Receiver closes
	rm, err := r.Receive()
	for ; err == nil; rm, err = r.Receive() {
		fmt.Printf("server received: %q\n", rm.Message.Body())
		rm.Accept() // Signal to the client that the message was accepted
	}
	fmt.Printf("server receiver closed: %v\n", err)
}

// Example client sending messages to a server running in a goroutine.
//
func Example_clientServer() {
	l, err := net.Listen("tcp", "127.0.0.1:0") // tcp4 so example will work on ipv6-disabled platforms
	if err != nil {
		log.Fatal(err)
	}

	// SERVER: start the server running in a separate goroutine
	var waitServer sync.WaitGroup // We will wait for the server goroutine to finish before exiting
	waitServer.Add(1)
	go func() { // Run the server in the background
		defer waitServer.Done()
		Server(l)
	}()

	// CLIENT: Send messages to the server
	addr := l.Addr()
	c, err := electron.Dial(addr.Network(), addr.String())
	if err != nil {
		log.Fatal(err)
	}
	s, err := c.Sender()
	if err != nil {
		log.Fatal(err)
	}
	for i := 0; i < 3; i++ {
		msg := fmt.Sprintf("hello %v", i)
		// Send and wait for the Outcome from the server.
		// Note: For higher throughput, use SendAsync() to send a stream of messages
		// and process the returning stream of Outcomes concurrently.
		s.SendSync(amqp.NewMessageWith(msg))
	}
	c.Close(nil) // Closing the connection will stop the server

	waitServer.Wait() // Let the server finish

	// Output:
	// server received: "hello 0"
	// server received: "hello 1"
	// server received: "hello 2"
	// server receiver closed: EOF
}
