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

import java.nio.ByteBuffer;

import org.apache.qpid.proton.codec.Data;

interface Element<T>
{
    int size();
    T getValue();
    Data.DataType getDataType();
    int encode(ByteBuffer b);
    Element next();
    Element prev();
    Element child();
    Element parent();

    void setNext(Element elt);
    void setPrev(Element elt);
    void setParent(Element elt);
    void setChild(Element elt);

    Element replaceWith(Element elt);

    Element addChild(Element element);
    Element checkChild(Element element);

    boolean canEnter();

    void render(StringBuilder sb);

}
