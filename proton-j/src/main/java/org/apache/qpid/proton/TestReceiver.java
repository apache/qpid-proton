package org.apache.qpid.proton;
import java.io.IOException;

import org.apache.qpid.proton.message2.Message;
import org.apache.qpid.proton.messenger.Messenger;


public class TestReceiver
{
    public static void main(String[] args) throws IOException
    {
        Messenger messenger = Proton.messenger();
        messenger.start();
        messenger.subscribe("amqp://~localhost:5672");
        messenger.recv(); 
        Message m = messenger.get();
        System.out.println("Received " + m);
    }

}
