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

abstract class AtomicElement<T> extends AbstractElement<T>
{

    AtomicElement(Element parent, Element prev)
    {
        super(parent, prev);
    }

    @Override
    public Element child()
    {
        throw new UnsupportedOperationException();
    }

    @Override
    public void setChild(Element elt)
    {
        throw new UnsupportedOperationException();
    }


    @Override
    public boolean canEnter()
    {
        return false;
    }

    @Override
    public Element checkChild(Element element)
    {
        throw new UnsupportedOperationException();
    }

    @Override
    public Element addChild(Element element)
    {
        throw new UnsupportedOperationException();
    }

    @Override
    String startSymbol() {
        throw new UnsupportedOperationException();
    }

    @Override
    String stopSymbol() {
        throw new UnsupportedOperationException();
    }

}
