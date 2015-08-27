package org.apache.qpid.proton;
import java.io.IOException;

import org.apache.qpid.proton.Proton;
import org.apache.qpid.proton.amqp.UnsignedByte;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.amqp.UnsignedShort;
import org.apache.qpid.proton.amqp.messaging.AmqpValue;
import org.apache.qpid.proton.message.Message;
import org.apache.qpid.proton.messenger.Messenger;


public class TestReceiver
{
    public static void main(String[] args) throws IOException
    {
        System.out.println("UnsignedInteger.MAX_VALUE : " + UnsignedInteger.MAX_VALUE);
        System.out.println("UnsignedShort.MAX_VALUE : " + UnsignedShort.MAX_VALUE);
        System.out.println("UnsignedShort.-1 : " + UnsignedShort.valueOf((short)-1));
        Messenger messenger = Proton.messenger();
        messenger.start();
        messenger.subscribe("amqp://~localhost:5672");
        messenger.recv();        
    }

}
