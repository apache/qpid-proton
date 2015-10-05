package org.apache.qpid.proton;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.qpid.proton.message2.Message;
import org.apache.qpid.proton.messenger.Messenger;

public class BenchmarkSender
{
    static String largeString = "Knowing that millions of people around the world would be watching in person and on television and expecting great";
    static String smallString = "Hello world";
    static long n = Long.MAX_VALUE;

    public static Message getMessage(int op)
    {
        Message m = Proton.message2();
        m.setAddress("amqp://localhost:5672");
        switch (op)
        {
        case 0 :
            m.setBody(smallString);
            break;
        case 1 :
            m.setBody(largeString);
            break;
        case 2 :
            m.setBody(n);
            break;
        case 3 :
           List<String> listSm = new ArrayList<String>();
           for (int i=0; i<5; i++)
           {
               listSm.add(smallString);
           } 
           m.setBody(listSm);
           break;
        case 4 :
            List<String> listBig = new ArrayList<String>();
            for (int i=0; i<100; i++)
            {
                listBig.add(smallString);
            } 
            m.setBody(listBig);
            break;
        case 5:
          Map<String, Integer> mapSm = new HashMap<String, Integer>(); 
          for (int i=0; i<5; i++)
          {
              mapSm.put(String.valueOf(i), i);
          }  
          m.setBody(mapSm);
          break;
        case 6:
            Map<String, Integer> mapBig = new HashMap<String, Integer>(); 
            for (int i=0; i<100; i++)
            {
                mapBig.put(String.valueOf(i), i);
            }  
            m.setBody(mapBig);
        case 7:
            List<Integer> l1 = Arrays.asList(1,2,3,4,5,6,7,8,9,10);
            Map<String, List<Integer>> nestedMapSm = new HashMap<String, List<Integer>>(); 
            for (int i=0; i<5; i++)
            {
                nestedMapSm.put(String.valueOf(i), l1);
            }
            m.setBody(nestedMapSm);
            break;
        case 8:
            List<Integer> l2 = Arrays.asList(1,2,3,4,5,6,7,8,9,10);
            Map<String, List<Integer>> nestedMapBig = new HashMap<String, List<Integer>>(); 
            for (int i=0; i<100; i++)
            {
                nestedMapBig.put(String.valueOf(i), l2);
            }
            m.setBody(nestedMapBig);
            break;
        }
        return m;
    }
    
    public static void main(String[] args) throws Exception
    {
        int count = Integer.getInteger("count", 1);
        int op = Integer.getInteger("op", 4);
        
        Message msg = getMessage(op);
        Messenger sender = Proton.messenger();
        sender.start();
        
        long start = System.nanoTime();
        for (int i=0; i<count; i++)
        {
            msg.setCreationTime(start);
            sender.put(msg);
        }
        sender.send();

        double elapsed = (System.nanoTime() - start)/1000000; 
        System.out.println(String.format("Time elapsed for sending %s msgs for op [%s] : %s (milli secs)", count, op, elapsed));
        
        final Object ob = new Object();
        synchronized (ob)
        {
            ob.wait();
        }
    }
}