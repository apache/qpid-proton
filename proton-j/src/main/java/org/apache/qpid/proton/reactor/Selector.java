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

import java.io.IOException;
import java.util.Iterator;

/**
 * A multiplexor of instances of {@link Selectable}.
 * <p>
 * Many instances of <code>Selectable</code> can be added to a selector, and
 * the {@link #select(long)} method used to block the calling thread until
 * one of the <code>Selectables</code> becomes read to perform an operation.
 * <p>
 * This class is not thread safe, so only one thread should be manipulating the
 * contents of the selector, or running the {@link #select(long)} method at
 * any given time.
 */
public interface Selector {

    /**
     * Adds a selectable to the selector.
     * @param selectable
     * @throws IOException
     */
    void add(Selectable selectable) throws IOException;

    /**
     * Updates the selector to reflect any changes interest by the specified
     * selectable.  This is achieved by calling the
     * {@link Selectable#isReading()} and {@link Selectable#isWriting()}
     * methods.
     * @param selectable
     */
    void update(Selectable selectable);

    /**
     * Removes a selectable from the selector.
     * @param selectable
     */
    void remove(Selectable selectable);

    /**
     * Waits for the specified timeout period for one or more selectables to
     * become ready for an operation.  Selectables that become ready are
     * returned by the {@link #readable()}, {@link #writeable()},
     * {@link #expired()}, or {@link #error()} methods.
     *
     * @param timeout the maximum number of milliseconds to block the calling
     *                thread waiting for a selectable to become ready for an
     *                operation.  The value zero is interpreted as check but
     *                don't block.
     * @throws IOException
     */
    void select(long timeout) throws IOException;

    /**
     * @return the selectables that have become readable since the last call
     *         to {@link #select(long)}.  Calling <code>select</code> clears
     *         any previous values in this set before adding new values
     *         corresponding to those selectables that have become readable.
     */
    Iterator<Selectable> readable();

    /**
     * @return the selectables that have become writable since the last call
     *         to {@link #select(long)}.  Calling <code>select</code> clears
     *         any previous values in this set before adding new values
     *         corresponding to those selectables that have become writable.
     */
    Iterator<Selectable> writeable();

    /**
     * @return the selectables that have expired since the last call
     *         to {@link #select(long)}.  Calling <code>select</code> clears
     *         any previous values in this set before adding new values
     *         corresponding to those selectables that have now expired.
     */
    Iterator<Selectable> expired();

    /**
     * @return the selectables that have encountered an error since the last
     *         call to {@link #select(long)}.  Calling <code>select</code>
     *         clears any previous values in this set before adding new values
     *         corresponding to those selectables that have encountered an
     *         error.
     */
    Iterator<Selectable> error() ;

    /** Frees the resources used by this selector. */
    void free();
}
