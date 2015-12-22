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

/**
 * A typesafe convenience class for associating additional data with {@link Extendable} classes.
 * <p>
 * An instance of <code>ExtendableAccessor</code> uses itself as the key in the {@link Extendable#attachments()}
 * so it's best instantiated as a static final member.
 * <pre><code>
 *   class Foo extends BaseHandler {
 *     private static ExtendableAccessor&lt;Link, Bar&gt; LINK_BAR = new ExtendableAccessor&lt;&gt;(Bar.class);
 *     void onLinkRemoteOpen(Event e) {
 *       Bar bar = LINK_BAR.get(e.getLink());
 *       if (bar == null) {
 *         bar = new Bar();
 *         LINK_BAR.set(e.getLink(), bar);
 *         }
 *       }
 *     }
 * </code></pre>
 * 
 * @param <E> An {@link Extendable} type where the data is to be stored
 * @param <T> The type of the data to be stored
 */
public final class ExtendableAccessor<E extends Extendable, T> {
    private final Class<T> klass;
    public ExtendableAccessor(Class<T> klass) {
        this.klass = klass;
    }

    public T get(E e) {
        return e.attachments().get(this, klass);
    }

    public void set(E e, T value) {
        e.attachments().set(this, klass, value);
    }
}