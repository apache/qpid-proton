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

import org.apache.qpid.proton.engine.Record;
import java.util.HashMap;
import java.util.Map;


/**
 * RecordImpl
 *
 */

public class RecordImpl implements Record
{

    private Map<Object,Object> values = new HashMap<Object,Object>();

    public <T> void set(Object key, Class<T> klass, T value) {
        values.put(key, value);
    }

    public <T> T get(Object key, Class<T> klass) {
        return klass.cast(values.get(key));
    }

    public void clear() {
        values.clear();
    }

    void copy(RecordImpl src) {
        values.putAll(src.values);
    }

}
