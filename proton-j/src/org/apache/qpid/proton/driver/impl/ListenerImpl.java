package org.apache.qpid.proton.driver.impl;

import java.io.IOException;
import java.nio.channels.ServerSocketChannel;
import java.nio.channels.SocketChannel;
import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Listener;

class ListenerImpl<C> implements Listener<C>
{
    private final C _context;
    private final ServerSocketChannel _channel;
    private final DriverImpl _driver;

    ListenerImpl(DriverImpl driver, ServerSocketChannel c, C context)
    {
        _driver = driver;
        _channel = c;
        _context = context;
    }

    public Connector accept()
    {
        try
        {
            SocketChannel c = _channel.accept();
            if(c != null)
            {
                c.configureBlocking(false);
                return new ServerConnectorImpl(_driver,c);
            }
        }
        catch (IOException e)
        {
            e.printStackTrace();  // TODO - Implement
        }
        return null;  //TODO - Implement
    }

    public C getContext()
    {
        return _context;
    }

    public void close()
    {
        //TODO - Implement
    }

    public void destroy()
    {
        //TODO - Implement
    }
}
