package org.apache.qpid.proton.messenger.impl;

import java.util.concurrent.atomic.AtomicBoolean;

import org.apache.qpid.proton.messenger.Listener;

public class ListenerImpl implements Listener
{
    private String _hostName;

    private int _port;

    private AtomicBoolean _closed = new AtomicBoolean(false);

    private AtomicBoolean _completed = new AtomicBoolean(false);

    private Object _ctx;

    private SocketListener _socketListener;

    ListenerImpl(String hostName, int port)
    {
        _hostName = hostName;
        _port = port;
    }

    @Override
    public String getHost()
    {
        return _hostName;
    }

    @Override
    public int getPort()
    {
        return _port;
    }

    @Override
    public void close()
    {
        _closed.set(true);
    }

    @Override
    public boolean isClosed()
    {
        return _closed.get();
    }

    @Override
    public void markCompleted()
    {
        _completed.set(true);
    }

    @Override
    public boolean isCompleted()
    {
        return _completed.get();
    }

    void setContext(Object ctx)
    {
        _ctx = ctx;
    }

    Object getContext()
    {
        return _ctx;
    }

    /*----------------------------------------
     * Used in active mode (passive == false)
     * Visible only to the impl classes
     * ---------------------------------------
     */
    void setSocketListener(SocketListener socket)
    {
        _socketListener = socket;
    }

    SocketListener getSocketListener()
    {
        return _socketListener;
    }
}