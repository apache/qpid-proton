package org.apache.qpid.proton;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.apache.qpid.proton.amqp.messaging.AmqpValue;

public class BenchmarkSender
{
    static String largeString = "Knowing that millions of people around the world would be watching in person and on television and expecting great";

    static String smallString = "Hello world";

    static long n = Long.MAX_VALUE;

    enum OP {STR_SMALL, STR_BIG, LONG, LIST_SMALL, LIST_BIG, MAP_SMALL, MAP_BIG, NESTED_SMALL, NESTED_BIG};
 
    public static Object getPayload(int op)
    {
        switch (op)
        {
        case 0:
            return smallString;
        case 1:
            return largeString;
        case 2:
            return n;
        case 3:
            List<String> listSm = new ArrayList<String>();
            for (int i = 0; i < 5; i++)
            {
                listSm.add(smallString);
            }
            return listSm;
        case 4:
            List<String> listBig = new ArrayList<String>();
            for (int i = 0; i < 100; i++)
            {
                listBig.add(smallString);
            }
            return listBig;
        case 5:
            Map<String, Integer> mapSm = new HashMap<String, Integer>();
            for (int i = 0; i < 5; i++)
            {
                mapSm.put(String.valueOf(i), i);
            }
            return mapSm;
        case 6:
            Map<String, Integer> mapBig = new HashMap<String, Integer>();
            for (int i = 0; i < 100; i++)
            {
                mapBig.put(String.valueOf(i), i);
            }
            return mapBig;
        case 7:
            List<Integer> l1 = Arrays.asList(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
            Map<String, List<Integer>> nestedMapSm = new HashMap<String, List<Integer>>();
            for (int i = 0; i < 5; i++)
            {
                nestedMapSm.put(String.valueOf(i), l1);
            }
            return nestedMapSm;
        case 8:
            List<Integer> l2 = Arrays.asList(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
            Map<String, List<Integer>> nestedMapBig = new HashMap<String, List<Integer>>();
            for (int i = 0; i < 100; i++)
            {
                nestedMapBig.put(String.valueOf(i), l2);
            }
            return nestedMapBig;
        default:
            return smallString;
        }
    }

    static void messengerOld(int count, int op) throws Exception
    {
        Object obj = getPayload(op);
        org.apache.qpid.proton.message.Message msg = Proton.message();
        msg.setAddress("amqp://localhost:5672");
        msg.setBody(new AmqpValue(obj));
        org.apache.qpid.proton.messenger.Messenger sender = new org.apache.qpid.proton.messenger.impl.MessengerImpl();
        sender.start();

        long start = System.nanoTime();
        for (int i = 0; i < count; i++)
        {
            msg.setCreationTime(start);
            sender.put(msg);
        }
        sender.send();

        double elapsed = (System.nanoTime() - start) / 1000000;
        System.out.println(String.format("Old codec Time elapsed for sending %s msgs for op [%s] : %s (milli secs)", count, OP.values()[op],
                elapsed));
    }

    static void messengerNew(int count, int op) throws Exception
    {
        Object obj = getPayload(op);
        org.apache.qpid.proton.message2.Message msg = Proton.message2();
        msg.setAddress("amqp://localhost:5672");
        msg.setBody(obj);
        org.apache.qpid.proton.messenger.Messenger2 sender = new org.apache.qpid.proton.messenger.impl.MessengerImpl2();
        sender.start();

        long start = System.nanoTime();
        for (int i = 0; i < count; i++)
        {
            msg.setCreationTime(start);
            sender.put(msg);
        }
        sender.send();

        double elapsed = (System.nanoTime() - start) / 1000000;
        System.out.println(String.format("New Codec Time elapsed for sending %s msgs for op [%s] : %s (milli secs)", count, op,
                elapsed));
    }

    public static void main(String[] args) throws Exception
    {
        int count = Integer.getInteger("count", 1);
        int op = Integer.getInteger("op", 1);

        boolean isNewCodec = Boolean.getBoolean("new-codec");

        if (isNewCodec)
        {
            messengerNew(count, op);
        }
        else
        {
            messengerOld(count, op);
        }

        final Object ob = new Object();
        synchronized (ob)
        {
            ob.wait();
        }
    }
}