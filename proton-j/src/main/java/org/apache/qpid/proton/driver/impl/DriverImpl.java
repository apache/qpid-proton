package org.apache.qpid.proton.driver.impl;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.nio.channels.*;
import java.util.Collections;
import java.util.Iterator;
import java.util.Set;
import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Driver;
import org.apache.qpid.proton.driver.Listener;

public class DriverImpl implements Driver
{
    private Selector _selector;
    private Set<SelectionKey> _selectedKeys = Collections.emptySet();

    public DriverImpl() throws IOException
    {
        _selector = Selector.open();
    }

    public void wakeup()
    {
        _selector.wakeup();
    }

    public void doWait(int timeout)
    {
        try
        {
            _selector.select(timeout);
            _selectedKeys = _selector.selectedKeys();
        }
        catch (IOException e)
        {
            e.printStackTrace();  // TODO - Implement
        }
    }

    public Listener listener()
    {
        Listener listener = null;
        listener = getFirstListener();
        if(listener == null)
        {
            try
            {
                selectNow();
            }
            catch (IOException e)
            {
                e.printStackTrace();  // TODO - Implement
            }
            listener = getFirstListener();
        }
        return listener;
    }

    private void selectNow() throws IOException
    {
        _selector.selectNow();
        _selectedKeys = _selector.selectedKeys();
    }

    private Listener getFirstListener()
    {
        Iterator<SelectionKey> selectedIter = _selectedKeys.iterator();

        while(selectedIter.hasNext())
        {
            SelectionKey key = selectedIter.next();
            selectedIter.remove();
            if(key.isAcceptable())
            {
                selectedIter.remove();
                return (Listener) key.attachment();

            }
        }
        return null;
    }

    public Connector connector()
    {
        Connector connector = null;
        connector = getFirstConnector();
        if(connector == null)
        {
            try
            {
                selectNow();
            }
            catch (IOException e)
            {
                e.printStackTrace();  // TODO - Implement
            }
            connector = getFirstConnector();
        }
        return connector;
    }

    private Connector getFirstConnector()
    {
        Iterator<SelectionKey> selectedIter = _selectedKeys.iterator();

        while(selectedIter.hasNext())
        {
            SelectionKey key = selectedIter.next();
            selectedIter.remove();
            if(key.isReadable() || key.isWritable())
            {
                selectedIter.remove();
                return (Connector) key.attachment();

            }
        }
        return null;
    }


    public void destroy()
    {
        //TODO - Implement
    }

    public <C> Listener<C> createListener(String host, int port, C context)
    {
        try
        {
            ServerSocketChannel serverSocketChannel = ServerSocketChannel.open();
            serverSocketChannel.configureBlocking(false);
            ServerSocket serverSocket = serverSocketChannel.socket();
            serverSocket.bind(new InetSocketAddress(host, port));
            return createListener(serverSocketChannel, context);
        }
        catch (ClosedChannelException e)
        {
            e.printStackTrace();  // TODO - Implement
        }
        catch (IOException e)
        {
            e.printStackTrace();  // TODO - Implement
        }
        return null;
    }

    public <C> Listener<C> createListener(ServerSocketChannel c, C context)
    {
        try
        {
            c.register(_selector, SelectionKey.OP_ACCEPT);
            return new ListenerImpl<C>(this, c, context);
        }
        catch (ClosedChannelException e)
        {
            e.printStackTrace();  // TODO - Implement
            throw new RuntimeException(e);
        }
    }

    public <C> Connector<C> createConnector(String host, int port, C context)
    {
        return null;  //TODO - Implement
    }

    public <C> Connector<C> createConnector(SelectableChannel fd, C context)
    {
        return null;  //TODO - Implement
    }
}
