package org.apache.qpid.proton.driver.impl;

import java.nio.channels.SocketChannel;
import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Listener;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.impl.SaslServerImpl;

class ServerConnectorImpl implements Connector
{
    private final SaslServerImpl _sasl;
    private DriverImpl _driver;
    private SocketChannel _channel;

    ServerConnectorImpl(DriverImpl driver, SocketChannel c)
    {
        _driver = driver;
        _channel = c;
        _sasl = new SaslServerImpl();
    }

    public void process()
    {
        //TODO - Implement
    }

    public Listener listener()
    {
        return null;  //TODO - Implement
    }

    public Sasl sasl()
    {
        return null;  //TODO - Implement
    }

    public Connection getConnection()
    {
        return null;  //TODO - Implement
    }

    public void setConnection(Connection connection)
    {
        //TODO - Implement
    }

    public Object getContext()
    {
        return null;  //TODO - Implement
    }

    public void setContext(Object context)
    {
        //TODO - Implement
    }

    public void close()
    {
        //TODO - Implement
    }

    public boolean isClosed()
    {
        return false;  //TODO - Implement
    }

    public void destroy()
    {
        //TODO - Implement
    }
}
