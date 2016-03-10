package org.apache.qpid.proton.engine.impl;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * Created by zolvarga on 2016-02-22.
 */
public class WebSocketTools
{
    final protected static char[] hexArray = "0123456789ABCDEF".toCharArray();

    private static String createUpgradeRequestEcho()
    {
        String host = "echo.websocket.org"; // 168.61.54.255
        String path = "";

        String key = "xuQy3IC/xr6VBhMS6QWOeQ=a";
        String endOfLine = "\r\n";
        StringBuilder stringBuilder = new StringBuilder().append("GET /").append(path).append(" HTTP/1.1").append(endOfLine).append("Connection: Upgrade").append(endOfLine).append("Host: ").append(host + ":80").append(endOfLine).append("Sec-WebSocket-Key: ").append(key).append(endOfLine).append("Sec-WebSocket-Version: 13").append(endOfLine).append("Upgrade: websocket").append(endOfLine).append(endOfLine);

        String upgradeRequest = stringBuilder.toString();
        return upgradeRequest;
    }

    public static String bytesToHex(byte[] bytes, int max)
    {
        char[] hexChars = new char[bytes.length * 5];
        for (int j = 0; j < max; j++)
        {
            int v = bytes[j] & 0xFF;
            hexChars[j * 5] = '0';
            hexChars[j * 5 + 1] = 'x';
            hexChars[j * 5 + 2] = hexArray[v >>> 4];
            hexChars[j * 5 + 3] = hexArray[v & 0x0F];
            hexChars[j * 5 + 4] = ' ';
        }
        return new String(hexChars);
    }
    //
    //    public static void clearLogFile() throws IOException
    //    {
    //        PrintWriter printWriter = new PrintWriter(new FileWriter(new File("E:/Documents/java-proton-j-websocket/log.txt"), false));
    //        printWriter.close();
    //    }

    public static void printBuffer(String title, ByteBuffer buffer) throws IOException
    {
        //        PrintWriter printWriter = new PrintWriter(new FileWriter(new File("E:/Documents/java-proton-j-websocket/log.txt"), true));

        int max = 10;
        if ((buffer.limit() > 0) && (buffer.limit() < buffer.capacity()))
        {
            System.out.println(title + " : ");
            //            printWriter.println(title + " : ");

            int size = buffer.limit();
            int pos = buffer.position();
            byte[] bytes = new byte[size - pos];
            buffer.get(bytes);
            //            System.out.println(WebSocketHandlerImpl.bytesToHex(bytes, max));
            //            if (max > buffer.limit())
            //            {
            //                max = buffer.limit();
            //            }
            //            printWriter.println(WebSocketHandlerImpl.bytesToHex(bytes, max));
            System.out.println("size=" + bytes.length);
            //            printWriter.println("size=" + bytes.length);

            for (int i = 0; i < bytes.length; i++)
            {
                System.out.print((char) bytes[i]);
                //                printWriter.write((char) bytes[i]);
            }
            System.out.println();
            //            printWriter.println();
            System.out.println("***************************************************");
            //            printWriter.println("***************************************************");
            //            printWriter.close();

            buffer.position(pos);
        }
    }
}
