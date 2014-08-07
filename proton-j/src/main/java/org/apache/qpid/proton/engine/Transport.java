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
package org.apache.qpid.proton.engine;

import java.nio.ByteBuffer;

import org.apache.qpid.proton.engine.impl.TransportImpl;


/**
 * <p>
 * Operates on the entities in the associated {@link Connection}
 * by accepting and producing binary AMQP output, potentially
 * layered within SASL and/or SSL.
 * </p>
 * <p>
 * After a connection is bound with {@link #bind(Connection)}, the methods for accepting and producing
 * output are typically repeatedly called. See the specific methods for details of their legal usage.
 * </p>
 * <p>
 * <strong>Processing the input data received from another AMQP container.</strong>
 * <ol>
 * <li>{@link #getInputBuffer()} </li>
 * <li>Write data into input buffer</li>
 * <li>{@link #processInput()}</li>
 * <li>Check the result, e.g. by calling {@link TransportResult#checkIsOk()}</li>
 * </ol>
 * </p>
 * <p>
 * <strong>Getting the output data to send to another AMQP container:</strong>
 * <ol>
 * <li>{@link #getOutputBuffer()} </li>
 * <li>Read output from output buffer</li>
 * <li>{@link #outputConsumed()}</li>
 * </ol>
 * </p>
 *
 * <p>The following methods on the byte buffers returned by {@link #getInputBuffer()} and {@link #getOutputBuffer()}
 * must not be called:
 * <ol>
 * <li> {@link ByteBuffer#clear()} </li>
 * <li> {@link ByteBuffer#compact()} </li>
 * <li> {@link ByteBuffer#flip()} </li>
 * <li> {@link ByteBuffer#mark()} </li>
 * </ol>
 * </p>
 */
public interface Transport extends Endpoint
{

    public static final class Factory
    {
        public static Transport create() {
            return new TransportImpl();
        }
    }

    public static final int TRACE_OFF = 0;
    public static final int TRACE_RAW = 1;
    public static final int TRACE_FRM = 2;
    public static final int TRACE_DRV = 4;

    public static final int DEFAULT_MAX_FRAME_SIZE = -1;

    /** the lower bound for the agreed maximum frame size (in bytes). */
    public int MIN_MAX_FRAME_SIZE = 512;
    public int SESSION_WINDOW = 16*1024;
    public int END_OF_STREAM = -1;

    public void trace(int levels);

    public void bind(Connection connection);
    public void unbind();

    public int capacity();
    public ByteBuffer tail();
    public void process() throws TransportException;
    public void close_tail();


    public int pending();
    public ByteBuffer head();
    public void pop(int bytes);
    public void close_head();

    public boolean isClosed();

    /**
     * Processes the provided input.
     *
     * @param bytes input bytes for consumption
     * @param offset the offset within bytes where input begins
     * @param size the number of bytes available for input
     *
     * @return the number of bytes consumed
     * @throws TransportException if the input is invalid, if the transport is already in an error state,
     * or if the input is empty (unless the remote connection is already closed)
     * @deprecated use {@link #getInputBuffer()} and {@link #processInput()} instead.
     */
    @Deprecated
    public int input(byte[] bytes, int offset, int size);

    /**
     * Get a buffer that can be used to write input data into the transport.
     * Once the client has finished putting into the input buffer, {@link #processInput()}
     * must be called.
     *
     * Successive calls to this method are not guaranteed to return the same object.
     * Once {@link #processInput()} is called the buffer must not be used.
     *
     * @throws TransportException if the transport is already in an invalid state
     */
    ByteBuffer getInputBuffer();

    /**
     * Tell the transport to process the data written to the input buffer.
     *
     * If the returned result indicates failure, the transport will not accept any more input.
     * Specifically, any subsequent {@link #processInput()} calls on this object will
     * throw an exception.
     *
     * @return the result of processing the data, which indicates success or failure.
     * @see #getInputBuffer()
     */
    TransportResult processInput();

    /**
     * Has the transport produce up to size bytes placing the result
     * into dest beginning at position offset.
     *
     * @param dest array for output bytes
     * @param offset the offset within bytes where output begins
     * @param size the maximum number of bytes to be output
     *
     * @return the number of bytes written
     * @deprecated use {@link #getOutputBuffer()} and {@link #outputConsumed()} instead
     */
    @Deprecated
    public int output(byte[] dest, int offset, int size);

    /**
     * Get a read-only byte buffer containing the transport's pending output.
     * Once the client has finished getting from the output buffer, {@link #outputConsumed()}
     * must be called.
     *
     * Successive calls to this method are not guaranteed to return the same object.
     * Once {@link #outputConsumed()} is called the buffer must not be used.
     *
     * If the transport's state changes AFTER calling this method, this will not be
     * reflected in the output buffer.
     */
    ByteBuffer getOutputBuffer();

    /**
     * Informs the transport that the output buffer returned by {@link #getOutputBuffer()}
     * is finished with, allowing implementation-dependent steps to be performed such as
     * reclaiming buffer space.
     */
    void outputConsumed();

    /**
     * Signal the transport to expect SASL frames used to establish a SASL layer prior to
     * performing the AMQP protocol version negotiation. This must first be performed before
     * the transport is used for processing. Subsequent invocations will return the same
     * {@link Sasl} object.
     *
     * @throws IllegalStateException if transport processing has already begun prior to initial invocation
     */
    Sasl sasl() throws IllegalStateException;

    /**
     * Wrap this transport's output and input to apply SSL encryption and decryption respectively.
     *
     * This method is expected to be called at most once. A subsequent invocation will return the same
     * {@link Ssl} object, regardless of the parameters supplied.
     *
     * @param sslDomain the SSL settings to use
     * @param sslPeerDetails may be null, in which case SSL session resume will not be attempted
     * @return an {@link Ssl} object representing the SSL session.
     */
    Ssl ssl(SslDomain sslDomain, SslPeerDetails sslPeerDetails);

    /**
     * As per {@link #ssl(SslDomain, SslPeerDetails)} but no attempt is made to resume a previous SSL session.
     */
    Ssl ssl(SslDomain sslDomain);


    /**
     * Get the maximum frame size for the transport
     *
     * @return the maximum frame size
     */
    int getMaxFrameSize();

    void setMaxFrameSize(int size);

    int getRemoteMaxFrameSize();

    int getChannelMax();

    void setChannelMax(int n);

    int getRemoteChannelMax();

}
