/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */
package org.apache.qpid.proton.engine.jni;

import static org.apache.qpid.proton.engine.TransportResultFactory.ok;
import static org.apache.qpid.proton.engine.TransportResultFactory.error;

import java.nio.ByteBuffer;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.ProtonUnsupportedOperationException;
import org.apache.qpid.proton.amqp.transport.ErrorCondition;
import org.apache.qpid.proton.engine.*;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_connection_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_error_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_ssl_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_transport_t;

public class JNITransport implements Transport
{
    private static final Logger _logger = Logger.getLogger(JNITransport.class.getName());

    private SWIGTYPE_p_pn_transport_t _impl;
    private JNISasl _sasl;
    private Object _context;
    private JNISsl _ssl;

    /**
     * Short-lived buffer to hold the input between calls to {@link #getInputBuffer()}
     * and {@link #processInput()}.
     * Null at all other times.
     */
    private ByteBuffer _inputBuffer;

    /**
     * Long-lived buffer to hold the output.
     */
    private ByteBuffer _outputBuffer = newReadableBuffer(0);

    public JNITransport()
    {
        _impl = Proton.pn_transport();
//        Proton.pn_transport_trace(_impl,Proton.PN_TRACE_FRM);
    }

    @Override
    @ProtonCEquivalent("pn_transport_bind")
    public void bind(Connection connection)
    {
        JNIConnection jniConn = (JNIConnection)connection;
        SWIGTYPE_p_pn_connection_t connImpl = jniConn.getImpl();
        Proton.pn_transport_bind(_impl, connImpl);
    }


    /*
     * Note that getInputBuffer, processInput, getOutputBuffer and outputProcessed
     * are all impplemented in terms of the old input/output API.
     * TODO re-implement them to use the new proton-c API (tail, capacity etc).
     * This will incidentally resolve PROTON-264 (Proton-c returns PN_EOS from pn_transport_input
     * after Close has been consumed, rather than reporting the number of bytes consumed).
     */

    /**
     * @return an input buffer sized to match the underlying transport's capacity
     */
    @Override
    @ProtonCEquivalent("pn_transport_tail")
    public ByteBuffer getInputBuffer()
    {
        int capacity = Proton.pn_transport_capacity(_impl);
        if(capacity < 0)
        {
            throw new TransportException("Cannot accept input because pn_transport_capacity returned " + capacity);
        }
        _inputBuffer = newWriteableBuffer(capacity);
        return _inputBuffer;
    }

    @Override
    @ProtonCEquivalent("pn_transport_push")
    public TransportResult processInput()
    {
        if(_inputBuffer == null)
        {
            throw new IllegalStateException("Called processInput without a preceding getInputBuffer call");
        }

        _inputBuffer.flip();
        try
        {
            int remaining = _inputBuffer.remaining();
            int numberConsumed = input(_inputBuffer.array(), 0, remaining);
            if(numberConsumed != remaining)
            {
                throw new TransportException(
                        "Transport accepted only " + numberConsumed
                        + " bytes of the input even though its previously advertised capacity was "
                        + remaining);
            }

            if(_logger.isLoggable(Level.FINE))
            {
                _logger.log(Level.FINE, this + " processed " + numberConsumed + " bytes");
            }
            return ok();
        }
        catch(TransportException e)
        {
            return error(e);
        }
        finally
        {
            _inputBuffer = null;
        }
    }

    /**
     * This method is public as it is used by Python layer.
     * This is a dummy implmentation that always returns ok because the actual checking is done by
     * proton-c inside {@link #input(byte[], int, int)}.
     *
     * @see Transport#input(byte[], int, int)
     */
    public TransportResult oldApiCheckStateBeforeInput(int inputLength)
    {
        return ok();
    }

    /*
     * Returns an output buffer with position == 0 and
     * limit == capacity == (numberOfPreviouslyLeftOverBytes + numberOfNewPendingBytes)
     */
    @Override
    @ProtonCEquivalent("pn_transport_head")
    public ByteBuffer getOutputBuffer()
    {
        ByteBuffer previousLeftOvers = _outputBuffer;
        int numberOfLeftovers = previousLeftOvers.remaining();

        int pending = Proton.pn_transport_pending(_impl);
        if(pending < 0)
        {
            if(_logger.isLoggable(Level.FINE))
            {
                _logger.log(Level.FINE, this + " is unable to produce more bytes. Pending result was: " + pending);
            }
            pending = 0;
        }

        if(_logger.isLoggable(Level.FINE))
        {
            _logger.log(Level.FINE, String.format(this + " will produce output consisting of %s leftovers and %s new bytes", numberOfLeftovers, pending));
        }
        _outputBuffer = newWriteableBuffer(numberOfLeftovers + pending);

        _outputBuffer.put(previousLeftOvers);

        int numberOfNewBytesWritten = output(_outputBuffer.array(), _outputBuffer.position(), pending);
        if(numberOfNewBytesWritten != pending)
        {
            throw new TransportException("Transport produced only " + numberOfNewBytesWritten
                    + " bytes of output even though it previously advertised that there were "
                    + pending + " pending");
        }

        _outputBuffer.rewind();
        _outputBuffer = _outputBuffer.asReadOnlyBuffer();
        return _outputBuffer;

    }

    @Override
    @ProtonCEquivalent("pn_transport_pop")
    public void outputConsumed()
    {
        if(_outputBuffer == null)
        {
            throw new IllegalStateException("Called outputConsumed without a preceding getOutputBuffer call");
        }
    }


    private ByteBuffer newWriteableBuffer(int capacity)
    {
        ByteBuffer newBuffer = ByteBuffer.allocate(capacity);
        return newBuffer;
    }

    private ByteBuffer newReadableBuffer(int capacity)
    {
        ByteBuffer newBuffer = ByteBuffer.allocate(capacity);
        newBuffer.flip();
        return newBuffer;
    }

    @Deprecated
    @Override
    @ProtonCEquivalent("pn_transport_input")
    public int input(byte[] bytes, int offset, int size)
    {
        int bytesConsumed = Proton.pn_transport_input(_impl, ByteBuffer.wrap(bytes, offset, size));
        if(bytesConsumed == Proton.PN_ERR)
        {
            SWIGTYPE_p_pn_error_t err = Proton.pn_transport_error(_impl);
            String errorText = Proton.pn_error_text(err);
            Proton.pn_error_clear(err);
            throw new TransportException(errorText);
        }
        else if (bytesConsumed == Proton.PN_EOS)
        {
            if(_logger.isLoggable(Level.FINE))
            {
                // TODO: PROTON-264 Proton-c returns PN_EOS after Close has been consumed
                _logger.fine(this + ": pn_transport_input returned EOS so we behave as if all the input has been consumed");
            }
            return size;
        }
        return bytesConsumed;
    }

    @Deprecated
    @Override
    @ProtonCEquivalent("pn_transport_output")
    public int output(byte[] bytes, int offset, int size)
    {
        int bytesProduced = Proton.pn_transport_output(_impl, ByteBuffer.wrap(bytes, offset, size));
        if (bytesProduced == Proton.PN_EOS)
        {
            // TODO: PROTON-264 Proton-c returns PN_EOS after Close has been consumed rather than
            // returning 0.
            return 0;
        }

        return bytesProduced;
    }


    @Override
    @ProtonCEquivalent("pn_sasl")
    public Sasl sasl()
    {
        if(_sasl == null)
        {
            _sasl = new JNISasl( Proton.pn_sasl(_impl));
        }
        return _sasl;

    }

    @Override
    @ProtonCEquivalent("pn_ssl")
    public Ssl ssl(SslDomain sslDomain, SslPeerDetails sslPeerDetails)
    {
        if(_ssl == null)
        {
            // TODO move this code to SslPeerDetails or its factory
            final String sessionId;
            if (sslPeerDetails == null)
            {
                sessionId = null;
            }
            else
            {
                sessionId = sslPeerDetails.getHostname() + ":" + sslPeerDetails.getPort();
            }

            SWIGTYPE_p_pn_ssl_t pn_ssl = Proton.pn_ssl( _impl );
            _ssl = new JNISsl( pn_ssl);
            Proton.pn_ssl_init(pn_ssl, ((JNISslDomain)sslDomain).getImpl(), sessionId);
            // TODO is the returned int an error code??
        }
        return _ssl;
    }

    @Override
    public Ssl ssl(SslDomain sslDomain)
    {
        return ssl(sslDomain, null);
    }

    @Override
    public EndpointState getLocalState()
    {
        return null; //TODO
    }

    @Override
    public EndpointState getRemoteState()
    {
        return null; //TODO
    }

    @Override
    public ErrorCondition getCondition()
    {
        return null; //TODO
    }

    @Override
    public void setCondition(ErrorCondition condition)
    {
        // TODO
    }

    @Override
    public ErrorCondition getRemoteCondition()
    {
        return null; //TODO
    }

    @Override
    public void free()
    {
        Proton.pn_transport_free(_impl);
    }

    @Override
    public void open()
    {
    }

    @Override
    public void close()
    {
    }

    @Override
    public void setContext(Object o)
    {
        _context = o;
    }

    @Override
    public void setMaxFrameSize(int size)
    {
        Proton.pn_transport_set_max_frame(_impl, (long) size);
    }

    @Override
    public int getMaxFrameSize()
    {
        return (int) Proton.pn_transport_get_max_frame(_impl);
    }


    @Override
    public Object getContext()
    {
        return _context;
    }

    @Override
    protected void finalize() throws Throwable
    {
        free();
        super.finalize();
    }
}
