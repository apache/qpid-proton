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

import java.util.List;
import java.util.LinkedList;
import java.util.Map;
import java.util.HashMap;
import java.util.Collection;
import java.util.Iterator;

import org.apache.qpid.proton.messenger.Status;
import org.apache.qpid.proton.messenger.Messenger;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.amqp.messaging.Accepted;
import org.apache.qpid.proton.amqp.messaging.Rejected;

class Store
{
    private static final Accepted ACCEPTED = Accepted.getInstance();
    private static final Rejected REJECTED = new Rejected();

    private LinkedList<StoreEntry> _store = new LinkedList<StoreEntry>();
    private HashMap<String, LinkedList<StoreEntry>> _stream = new HashMap<String, LinkedList<StoreEntry>>();

    // for incoming/outgoing window tracking
    int _window;
    int _lwm;
    int _hwm;
    private HashMap<Integer, StoreEntry> _tracked = new HashMap<Integer, StoreEntry>();

    Store()
    {
    }

    private boolean isTracking( Integer id )
    {
        return id != null && (id.intValue() - _lwm >= 0) && (_hwm - id.intValue() > 0);
    }

    int size()
    {
        return _store.size();
    }

    int getWindow()
    {
        return _window;
    }

    void setWindow(int window)
    {
        _window = window;
    }

    StoreEntry put(String address)
    {
        if (address == null) address = "";
        StoreEntry entry = new StoreEntry(this, address);
        _store.add( entry );
        LinkedList<StoreEntry> list = _stream.get( address );
        if (list != null) {
            list.add( entry );
        } else {
            list = new LinkedList<StoreEntry>();
            list.add( entry );
            _stream.put( address, list );
        }
        entry.stored();
        return entry;
    }

    StoreEntry get(String address)
    {
        if (address != null) {
            LinkedList<StoreEntry> list = _stream.get( address );
            if (list != null) return list.peekFirst();
        } else {
            return _store.peekFirst();
        }
        return null;
    }

    StoreEntry getEntry(int id)
    {
        return _tracked.get(id);
    }

    Iterator<StoreEntry> trackedEntries()
    {
        return _tracked.values().iterator();
    }

    void freeEntry(StoreEntry entry)
    {
        if (entry.isStored()) {
            _store.remove( entry );
            LinkedList<StoreEntry> list = _stream.get( entry.getAddress() );
            if (list != null) list.remove( entry );
            entry.notStored();
        }
        // note well: may still be in _tracked map if still in window!
    }

    public int trackEntry(StoreEntry entry)
    {
        assert( entry.getStore() == this );
        entry.setId(_hwm++);
        _tracked.put(entry.getId(), entry);
        slideWindow();
        return entry.getId();
    }

    private void slideWindow()
    {
        if (_window >= 0)
        {
            while (_hwm - _lwm > _window)
            {
                StoreEntry old = getEntry(_lwm);
                if (old != null)
                {
                    _tracked.remove( old.getId() );
                    Delivery d = old.getDelivery();
                    if (d != null) {
                        if (d.getLocalState() == null)
                            d.disposition(ACCEPTED);
                        d.settle();
                    }
                }
                _lwm++;
            }
        }
    }

    int update(int id, Status status, int flags, boolean settle, boolean match )
    {
        if (!isTracking(id)) return 0;

        int start = (Messenger.CUMULATIVE & flags) != 0 ? _lwm : id;
        for (int i = start; (id - i) >= 0; i++)
        {
            StoreEntry e = getEntry(i);
            if (e != null)
            {
                Delivery d = e.getDelivery();
                if (d != null)
                {
                    if (d.getLocalState() == null)
                    {
                        if (match)
                        {
                            d.disposition(d.getRemoteState());
                        }
                        else
                        {
                            switch (status)
                            {
                            case ACCEPTED:
                                d.disposition(ACCEPTED);
                                break;
                            case REJECTED:
                                d.disposition(REJECTED);
                                break;
                            default:
                                break;
                            }
                        }
                        e.updated();
                    }
                }
                if (settle)
                {
                    if (d != null)
                    {
                        d.settle();
                    }
                    _tracked.remove(e.getId());
                }
            }
        }

        while (_hwm - _lwm > 0 && !_tracked.containsKey(_lwm))
        {
            _lwm++;
        }

        return 0;
    }
}


