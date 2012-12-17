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
package org.apache.qpid.proton.messenger.impl;

import org.apache.qpid.proton.messenger.Tracker;

class TrackerImpl implements Tracker
{
    private boolean _outgoing;
    private int _sequence;

    TrackerImpl(boolean outgoing, int sequence)
    {
        _outgoing = outgoing;
        _sequence = sequence;
    }

    boolean isOutgoing()
    {
        return _outgoing;
    }

    int getSequence()
    {
        return _sequence;
    }

    public String toString()
    {
        return (_outgoing ? "O:" : "I:") + Integer.toString(_sequence);
    }
}
