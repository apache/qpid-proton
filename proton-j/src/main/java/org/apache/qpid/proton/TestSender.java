package org.apache.qpid.proton;
import java.io.IOException;

import org.apache.qpid.proton.message2.AmqpValue;
import org.apache.qpid.proton.message2.Message;
import org.apache.qpid.proton.messenger.Messenger;

public class TestSender
{
    public static void main(String[] args) throws IOException
    {
        Messenger messenger = Proton.messenger();
        messenger.start();
        for (int i=0; i < 1; i++)
        {
            Message m = Proton.message2();
            m.setAddress("amqp://localhost:5672");
            m.setBody(new AmqpValue("Msg" + i));
            messenger.put(m);
        }
        messenger.send();
    }
}