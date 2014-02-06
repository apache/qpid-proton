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

package org.apache.qpid.proton.codec.impl;

abstract class AbstractElement<T> implements Element<T>
{
    private Element _parent;
    private Element _next;
    private Element _prev;

    AbstractElement(Element parent, Element prev)
    {
        _parent = parent;
        _prev = prev;
    }

    protected boolean isElementOfArray()
    {
        return _parent instanceof ArrayElement && !(((ArrayElement)parent()).isDescribed() && this == _parent.child());
    }

    @Override
    public Element next()
    {
        // TODO
        return _next;
    }

    @Override
    public Element prev()
    {
        // TODO
        return _prev;
    }

    @Override
    public Element parent()
    {
        // TODO
        return _parent;
    }

    @Override
    public void setNext(Element elt)
    {
        _next = elt;
    }

    @Override
    public void setPrev(Element elt)
    {
        _prev = elt;
    }

    @Override
    public void setParent(Element elt)
    {
        _parent = elt;
    }

    @Override
    public Element replaceWith(Element elt)
    {
        if (_parent != null) {
            elt = _parent.checkChild(elt);
        }

        elt.setPrev(_prev);
        elt.setNext(_next);
        elt.setParent(_parent);

        if (_prev != null) {
            _prev.setNext(elt);
        }
        if (_next != null) {
            _next.setPrev(elt);
        }

        if (_parent != null && _parent.child() == this) {
            _parent.setChild(elt);
        }

        return elt;
    }

    @Override
    public String toString()
    {
        return String.format("%s[%h]{parent=%h, prev=%h, next=%h}",
                             this.getClass().getSimpleName(),
                             System.identityHashCode(this),
                             System.identityHashCode(_parent),
                             System.identityHashCode(_prev),
                             System.identityHashCode(_next));
    }

    abstract String startSymbol();

    abstract String stopSymbol();

    @Override
    public void render(StringBuilder sb)
    {
        if (canEnter()) {
            sb.append(startSymbol());
            Element el = child();
            boolean first = true;
            while (el != null) {
                if (first) {
                    first = false;
                } else {
                    sb.append(", ");
                }
                el.render(sb);
                el = el.next();
            }
            sb.append(stopSymbol());
        } else {
            sb.append(getDataType()).append(" ").append(getValue());
        }
    }

}
