package org.apache.qpid.proton;

public class BenchmarkReceiver
{
    static void messengerOld(int count) throws Exception
    {
        org.apache.qpid.proton.messenger.Messenger rec = new org.apache.qpid.proton.messenger.impl.MessengerImpl();
        rec.start();
        rec.subscribe("amqp://~localhost:5672");
        int i = 0;
        org.apache.qpid.proton.message.Message m = null;
        long start = 0;
        while (i < count)
        {
            rec.recv();
            while ((m = rec.get()) != null)
            {
                i++;
                start = m.getCreationTime();
            }
        }
        System.out.println("Count " + count + " i " + i);

        double elapsed = (System.nanoTime() - start) / 1000000;
        System.out.println(String.format("Old codec Time elapsed (end-to-end) for %s msgs : %s (milli secs)", count, elapsed));
    }

    
    static void messengerNew(int count)  throws Exception
    {
        org.apache.qpid.proton.messenger.Messenger2 rec = new org.apache.qpid.proton.messenger.impl.MessengerImpl2();
        rec.start();
        rec.subscribe("amqp://~localhost:5672");
        int i = 0;
        org.apache.qpid.proton.message2.Message m = null;
        long start = 0;
        while (i < count)
        {
            rec.recv();
            while ((m = rec.get()) != null)
            {
                i++;
                start = m.getCreationTime();
            }
        }

        double elapsed = (System.nanoTime() - start) / 1000000;
        System.out.println(String.format("New codec Time elapsed (end-to-end) for %s msgs : %s (milli secs)", count, elapsed));
    }
 
    public static void main(String[] args) throws Exception
    {
        int count = Integer.getInteger("count", 1);
        boolean isNewCodec = Boolean.getBoolean("new-codec");

        if (isNewCodec)
        {
            messengerNew(count);
        }
        else
        {
            messengerOld(count);
        }
    }
}