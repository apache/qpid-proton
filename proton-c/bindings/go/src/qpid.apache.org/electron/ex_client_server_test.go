package electron_test

import (
	"fmt"
	"net"
	"qpid.apache.org/amqp"
	"qpid.apache.org/electron"
)

//  Print errors
func check(msg string, err error) bool {
	if err != nil {
		fmt.Printf("%s: %s\n", msg, err)
	}
	return err == nil
}

func runServer(cont electron.Container, l net.Listener) {
	for c, err := cont.Accept(l); check("accept connection", err); c, err = cont.Accept(l) {
		go func() { // Process connections concurrently, accepting AMQP endpoints
			for in := range c.Incoming() {
				ep := in.Accept() // Accept all endpoints
				go func() {       // Process endpoints concurrently
					switch ep := ep.(type) {
					case electron.Sender:
						m := amqp.NewMessageWith("hello yourself")
						fmt.Printf("server %q sending %q\n", ep.Source(), m.Body())
						ep.SendForget(m) // One-way send, client does not need to Accept.
					case electron.Receiver:
						if rm, err := ep.Receive(); check("server receive", err) {
							fmt.Printf("server %q received %q\n", ep.Target(), rm.Message.Body())
							err := rm.Accept() // Client is waiting for Accept.
							check("accept message", err)
						}
					}
				}()
			}
		}()
	}
}

func startServer() (addr net.Addr) {
	cont := electron.NewContainer("server")
	if l, err := net.Listen("tcp", ""); check("listen", err) {
		addr = l.Addr()
		go runServer(cont, l)
	}
	return addr
}

// Connect to addr and send/receive a message.
func client(addr net.Addr) {
	if c, err := electron.Dial(addr.Network(), addr.String()); check("dial", err) {
		defer c.Close(nil)
		if s, err := c.Sender(electron.Target("target")); check("sender", err) {
			fmt.Printf("client sending\n")
			s.SendSync(amqp.NewMessageWith("hello")) // Send and wait for server to Accept()
		}
		if r, err := c.Receiver(electron.Source("source")); check("receiver", err) {
			if rm, err := r.Receive(); err == nil {
				fmt.Printf("client received %q\n", rm.Message.Body())
			}
		}
	}
}

// Example client and server communicating via AMQP over a TCP/IP connection.
//
// Normally client and server would be separate processes.
// For more realistic examples:
//     https://github.com/apache/qpid-proton/blob/master/examples/go/README.md
//
func Example_clientServer() {
	addr := startServer()
	client(addr)
	// Output:
	// client sending
	// server "target" received "hello"
	// server "source" sending "hello yourself"
	// client received "hello yourself"
}
