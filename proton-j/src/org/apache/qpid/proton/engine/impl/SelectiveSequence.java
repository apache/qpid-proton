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

package org.apache.qpid.proton.engine.impl;

import org.apache.qpid.proton.engine.Sequence;

import java.util.Collection;
import java.util.Iterator;

abstract class SelectiveSequence<E> implements Sequence<E>
{
    private final Sequence<E> _sequence;

    SelectiveSequence(Collection<E> collection)
    {
        this(collection.iterator());
    }
    
    SelectiveSequence(Iterator<E> iterator)
    {
        this(new IteratorSequence<E>(iterator));
    }

    
    SelectiveSequence(Sequence<E> sequence)
    {
        _sequence = sequence;
    }

    public E next()
    {
        E next;
        while((next = _sequence.next())!= null)
        {
            if(select(next))
            {
                return next;
            }
        }
        return null;
    }
    
    abstract protected boolean select(E next);
    
}
