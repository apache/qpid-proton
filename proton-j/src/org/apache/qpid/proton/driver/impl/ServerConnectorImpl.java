package org.apache.qpid.proton.driver.impl;

import static org.apache.qpid.proton.driver.impl.ServerConnectorImpl.ConnectorState.NEW;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.SelectionKey;
import java.nio.channels.SocketChannel;

import org.apache.qpid.proton.driver.Connector;
import org.apache.qpid.proton.driver.Listener;
import org.apache.qpid.proton.engine.Connection;
import org.apache.qpid.proton.engine.Sasl;
import org.apache.qpid.proton.engine.impl.SaslServerImpl;
import org.apache.qpid.proton.logging.LogHandler;

class ServerConnectorImpl<C> implements Connector<C>
{
    public static int END_OF_STREAM = -1;
    private static int DEFAULT_BUFFER_SIZE = 64 * 1024;
    private static int readBufferSize = Integer.getInteger
        ("pn.receive_buffer_size", DEFAULT_BUFFER_SIZE);
    private static int writeBufferSize = Integer.getInteger
        ("pn.send_buffer_size", DEFAULT_BUFFER_SIZE);

    enum ConnectorState {NEW, OPENED, CLOSED};

    private final SaslServerImpl _sasl;
    private final DriverImpl _driver;
    private final SocketChannel _channel;
    private final LogHandler _logger;
    private C _context;
    private Connection _connection;
    private SelectionKey _key;
    private ConnectorState _state = NEW;

    private ByteBuffer _readBuffer = ByteBuffer.allocate(readBufferSize);
    private int _bytesNotRead = 0;

    private int _bytesNotWritten = 0;
    private ByteBuffer _writeBuffer = ByteBuffer.allocate(writeBufferSize);

    ServerConnectorImpl(DriverImpl driver, SocketChannel c, C context, SelectionKey key)
    {
        _driver = driver;
        _channel = c;
        _sasl = new SaslServerImpl();
        _sasl.setMechanisms(new String[]{"ANONYMOUS"}); //TODO
        _logger = driver.getLogHandler();
        _context = context;
        _key = key;
    }

    public void process()
    {
        if (!_channel.isOpen())
        {
            return;
        }

        if (_key.isReadable())
        {
            read();
        }

        if (_key.isWritable())
        {
            write();
        }
    }

    int processInput(byte[] bytes, int offset, int size)
    {
        int read = 0;
        while (read < size)
        {
            switch (_state)
            {
            case NEW:
                read += readSasl(bytes, offset, size);
                writeSasl();
                break;
            case OPENED:
                read += readAMQPCommands(bytes, offset, size);
                writeAMQPCommands();
                break;
            }
        }
        return read;
    }

    void processOutput()
    {
        switch (_state)
        {
        case NEW:
            writeSasl();
            break;
        case OPENED:
            writeAMQPCommands();
            break;
        }
    }

    private int readAMQPCommands(byte[] bytes, int offset, int size)
    {
        int consumed = _connection.transport().input(bytes, offset, size);
        if (consumed == END_OF_STREAM)
        {
            return size;
        }
        else
        {
            return consumed;
        }
    }

    private void writeAMQPCommands()
    {
        int size = _writeBuffer.array().length - _bytesNotWritten;
        _bytesNotWritten += _connection.transport().output(_writeBuffer.array(),
                _bytesNotWritten, size);
    }

    private void setState(ConnectorState newState)
    {
        _state = newState;
    }

    int readSasl(byte[] bytes, int offset, int size)
    {
        int consumed = _sasl.input(bytes, offset, size);
        if (consumed == END_OF_STREAM)
        {
            return size;
        }
        else
        {
            return consumed;
        }
    }

    void writeSasl()
    {
        int size = _writeBuffer.array().length - _bytesNotWritten;
        _bytesNotWritten += _sasl.output(_writeBuffer.array(),
                _bytesNotWritten, size);
    }

    void write()
    {
        try
        {
            processOutput();
            if (_bytesNotWritten > 0)
            {
                _writeBuffer.limit(_bytesNotWritten);
                int written = _channel.write(_writeBuffer);
                if (_writeBuffer.hasRemaining())
                {
                    _writeBuffer.compact();
                    _bytesNotWritten = _bytesNotWritten - written;
                }
                else
                {
                    _writeBuffer.clear();
                    _bytesNotWritten = 0;
                }
            }
        }
        catch (IOException e)
        {
            e.printStackTrace();
        }
    }

    void read()
    {
        try
        {
            int  bytesRead = _channel.read(_readBuffer);
            int consumed = 0;
            while (bytesRead > 0)
            {
                consumed = processInput(_readBuffer.array(), 0, bytesRead + _bytesNotRead);
                if (consumed < bytesRead)
                {
                    _readBuffer.compact();
                    _bytesNotRead = bytesRead - consumed;
                }
                else
                {
                    _readBuffer.rewind();
                    _bytesNotRead = 0;
                }
                bytesRead = _channel.read(_readBuffer);
            }
        }
        catch (IOException e)
        {
            e.printStackTrace();
        }
    }

    public Listener listener()
    {
        return null;  //TODO - Implement
    }

    public Sasl sasl()
    {
        return _sasl;
    }

    public Connection getConnection()
    {
        return _connection;
    }

    public void setConnection(Connection connection)
    {
        if (_sasl.isDone())
        {
            writeSasl();
        }
        else
        {
            throw new RuntimeException("Cannot set the connection before authentication is completed");
        }

        _connection = connection;
        // write any initial data
        int size = _writeBuffer.array().length - _bytesNotWritten;
        _bytesNotWritten += _connection.transport().output(_writeBuffer.array(),
                _bytesNotWritten, size);
        setState(ConnectorState.OPENED);
    }

    public C getContext()
    {
        return _context;
    }

    public void setContext(C context)
    {
        _context = context;
    }

    public void close()
    {
        try
        {
            writeSasl();
            _channel.close();
        }
        catch (IOException e)
        {

        }
    }

    public boolean isClosed()
    {
        return !_channel.isOpen();
    }

    public void destroy()
    {
        close();
    }
}
