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
import org.apache.qpid.proton.messenger.Status;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.amqp.messaging.Accepted;
import org.apache.qpid.proton.amqp.messaging.Modified;
import org.apache.qpid.proton.amqp.messaging.Rejected;
import org.apache.qpid.proton.amqp.messaging.Released;
import org.apache.qpid.proton.amqp.messaging.Received;
import org.apache.qpid.proton.amqp.transport.DeliveryState;

class StoreEntry
{
    private Store  _store;
    private Integer _id;
    private String _address;
    private byte[] _encodedMsg;
    private int _encodedLength;
    private Delivery _delivery;
    private Status _status = Status.UNKNOWN;
    private Object _context;
    private boolean _inStore = false;

    public StoreEntry(Store store, String address)
    {
        _store = store;
        _address = address;
    }

    public Store getStore()
    {
        return _store;
    }

    public boolean isStored()
    {
        return _inStore;
    }

    public void stored()
    {
        _inStore = true;
    }

    public void notStored()
    {
        _inStore = false;
    }

    public String getAddress()
    {
        return _address;
    }

    public byte[] getEncodedMsg()
    {
        return _encodedMsg;
    }

    public int getEncodedLength()
    {
        return _encodedLength;
    }

    public void setEncodedMsg( byte[] encodedMsg, int length )
    {
        _encodedMsg = encodedMsg;
        _encodedLength = length;
    }

    public void setId(int id)
    {
        _id = new Integer(id);
    }

    public Integer getId()
    {
        return _id;
    }

    public void setDelivery( Delivery d )
    {
        if (_delivery != null)
        {
            _delivery.setContext(null);
        }
        _delivery = d;
        if (_delivery != null)
        {
            _delivery.setContext(this);
        }
        updated();
    }

    public Delivery getDelivery()
    {
        return _delivery;
    }

    public Status getStatus()
    {
        return _status;
    }

    public void setStatus(Status status)
    {
        _status = status;
    }

    private static Status _disp2status(DeliveryState disp)
    {
        if (disp == null) return Status.PENDING;

        if (disp instanceof Received)
            return Status.PENDING;
        if (disp instanceof Accepted)
            return Status.ACCEPTED;
        if (disp instanceof Rejected)
            return Status.REJECTED;
        if (disp instanceof Released)
            return Status.RELEASED;
        if (disp instanceof Modified)
            return Status.MODIFIED;
        assert(false);
        return null;
    }

    public void updated()
    {
        if (_delivery != null)
        {
            if (_delivery.getRemoteState() != null)
            {
                _status = _disp2status(_delivery.getRemoteState());
            }
            else if (_delivery.remotelySettled())
            {
                DeliveryState disp = _delivery.getLocalState();
                if (disp == null) {
                    _status = Status.SETTLED;
                } else {
                    _status = _disp2status(_delivery.getLocalState());
                }
            }
            else
            {
                _status = Status.PENDING;
            }
        }
    }

    public void setContext(Object context)
    {
        _context = context;
    }

    public Object getContext()
    {
        return _context;
    }
}
