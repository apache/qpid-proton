package org.apache.qpid.proton.engine.impl;
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


class LinkNode<E>
{
    public interface Query<T>
    {
        public boolean matches(LinkNode<T> node);
    }


    private E _value;
    private LinkNode<E> _prev;
    private LinkNode<E> _next;

    private LinkNode(E value)
    {
        _value = value;
    }

    public E getValue()
    {
        return _value;
    }

    public LinkNode<E> getPrev()
    {
        return _prev;
    }

    public LinkNode<E> getNext()
    {
        return _next;
    }

    public LinkNode<E> next(Query<E> query)
    {
        LinkNode<E> next = _next;
        while(next != null && !query.matches(next))
        {
            next = next.getNext();
        }
        return next;
    }

    public LinkNode<E> remove()
    {
        LinkNode<E> prev = _prev;
        LinkNode<E> next = _next;
        if(prev != null)
        {
            prev._next = next;
        }
        if(next != null)
        {
            next._prev = prev;
        }
        _next = _prev = null;
        return next;
    }

    public LinkNode<E> addAtTail(E value)
    {
        if(_next == null)
        {
            _next = new LinkNode<E>(value);
            _next._prev = this;
            return _next;
        }
        else
        {
            return _next.addAtTail(value);
        }
    }

    public static <T> LinkNode<T> newList(T value)
    {
        return new LinkNode<T>(value);
    }


}
