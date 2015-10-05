package org.apache.qpid.proton;

import org.apache.qpid.proton.message2.Message;
import org.apache.qpid.proton.messenger.Messenger;

public class BenchmarkReceiver
{   
    public static void main(String[] args) throws Exception
    {
        int count = Integer.getInteger("count", 1);
        
        Messenger rec = Proton.messenger();
        rec.start();
        rec.subscribe("amqp://~localhost:5672");
        int i = 0;
        Message m = null;
        long start = 0;
        while (i <count)
        {
            rec.recv();
            while ((m = rec.get()) != null)
            {
                i++;
                start = m.getCreationTime();
            }
        }    
        System.out.println("Count " + count + " i "  + i);
        
        double elapsed = (System.nanoTime() - start)/1000000; 
        System.out.println(String.format("Time elapsed (end-to-end) for %s msgs : %s (milli secs)", count, elapsed));
    }
}