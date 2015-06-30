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

package org.apache.qpid.proton.reactor;

import java.nio.channels.SelectableChannel;

import org.apache.qpid.proton.engine.Collector;
import org.apache.qpid.proton.engine.Extendable;

/**
 * An entity that can be multiplexed using a {@link Selector}.
 * <p>
 * Every selectable is associated with exactly one {@link SelectableChannel}.
 * Selectables may be interested in three kinds of events: read events, write
 * events, and timer events. A selectable will express its interest in these
 * events through the {@link #isReading()}, {@link #isWriting()}, and
 * {@link #getDeadline()} methods.
 * <p>
 * When a read, write, or timer event occurs, the selectable must be notified by
 * calling {@link #readable()}, {@link #writeable()}, or {@link #expired()} as
 * appropriate.
 *
 * Once a selectable reaches a terminal state (see {@link #isTerminal()}, it
 * will never be interested in events of any kind. When this occurs it should be
 * removed from the Selector and discarded using {@link #free()}.
 */
public interface Selectable extends ReactorChild, Extendable {

    /**
     * A callback that can be passed to the various "on" methods of the
     * selectable - to allow code to be run when the selectable becomes ready
     * for the associated operation.
     */
    interface Callback {
        void run(Selectable selectable);
    }

    /**
     * @return <code>true</code> if the selectable is interested in receiving
     *         notification (via the {@link #readable()} method that indicate
     *         that the associated {@link SelectableChannel} has data ready
     *         to be read from it.
     */
    boolean isReading();

    /**
     * @return <code>true</code> if the selectable is interested in receiving
     *         notifications (via the {@link #writeable()} method that indicate
     *         that the associated {@link SelectableChannel} is ready to be
     *         written to.
     */
    boolean isWriting();

    /**
     * @return a deadline after which this selectable can expect to receive
     *         a notification (via the {@link #expired()} method that indicates
     *         that the deadline has past.  The deadline is expressed in the
     *         same format as {@link System#currentTimeMillis()}.  Returning
     *         a deadline of zero (or a negative number) indicates that the
     *         selectable does not wish to be notified of expiry.
     */
    long getDeadline();

    /**
     * Sets the value that will be returned by {@link #isReading()}.
     * @param reading
     */
    void setReading(boolean reading);

    /**
     * Sets the value that will be returned by {@link #isWriting()}.
     * @param writing
     */
    void setWriting(boolean writing);

    /**
     * Sets the value that will be returned by {@link #getDeadline()}.
     * @param deadline
     */
    void setDeadline(long deadline);

    /**
     * Registers a callback that will be run when the selectable becomes ready
     * for reading.
     * @param runnable the callback to register.  Any previously registered
     *                 callback will be replaced.
     */
    void onReadable(Callback runnable);

    /**
     * Registers a callback that will be run when the selectable becomes ready
     * for writing.
     * @param runnable the callback to register.  Any previously registered
     *                 callback will be replaced.
     */
    void onWritable(Callback runnable);

    /**
     * Registers a callback that will be run when the selectable expires.
     * @param runnable the callback to register.  Any previously registered
     *                 callback will be replaced.
     */
    void onExpired(Callback runnable);

    /**
     * Registers a callback that will be run when the selectable is notified of
     * an error.
     * @param runnable the callback to register.  Any previously registered
     *                 callback will be replaced.
     */
    void onError(Callback runnable);

    /**
     * Registers a callback that will be run when the selectable is notified
     * that it has been released.
     * @param runnable the callback to register.  Any previously registered
     *                 callback will be replaced.
     */
    void onRelease(Callback runnable);

    /**
     * Registers a callback that will be run when the selectable is notified
     * that it has been free'd.
     * @param runnable the callback to register.  Any previously registered
     *                 callback will be replaced.
     */
    void onFree(Callback runnable);

    /**
     * Notify the selectable that the underlying {@link SelectableChannel} is
     * ready for a read operation.
     */
    void readable();

    /**
     * Notify the selectable that the underlying {@link SelectableChannel} is
     * ready for a write operation.
     */
    void writeable();

    /** Notify the selectable that it has expired. */
    void expired();

    /** Notify the selectable that an error has occurred. */
    void error();

    /** Notify the selectable that it has been released. */
    void release();

    /** Notify the selectable that it has been free'd. */
    @Override
    void free();

    /**
     * Associates a {@link SelectableChannel} with this selector.
     * @param channel
     */
    void setChannel(SelectableChannel channel); // This is the equivalent to pn_selectable_set_fd(...)

    /** @return the {@link SelectableChannel} associated with this selector. */
    SelectableChannel getChannel(); // This is the equivalent to pn_selectable_get_fd(...)

    /**
     * Check if a selectable is registered.  This can be used for tracking
     * whether a given selectable has been registerd with an external event
     * loop.
     * <p>
     * <em>Note:</em> the reactor code, currently, does not use this flag.
     * @return <code>true</code>if the selectable is registered.
     */
    boolean isRegistered();  // XXX: unused in C reactor code

    /**
     * Set the registered flag for a selectable.
     * <p>
     * <em>Note:</em> the reactor code, currently, does not use this flag.
     * @param registered the value returned by {@link #isRegistered()}
     */
    void setRegistered(boolean registered); // XXX: unused in C reactor code

    /**
     * Configure a selectable with a set of callbacks that emit readable,
     * writable, and expired events into the supplied collector.
     * @param collector
     */
    void setCollector(final Collector collector);

    /** @return the reactor to which this selectable is a child. */
    Reactor getReactor() ;

    /**
     * Terminates the selectable.  Once a selectable reaches a terminal state
     * it will never be interested in events of any kind.
     */
    public void terminate() ;

    /**
     * @return <code>true</code> if the selectable has reached a terminal state.
     */
    boolean isTerminal();

}
