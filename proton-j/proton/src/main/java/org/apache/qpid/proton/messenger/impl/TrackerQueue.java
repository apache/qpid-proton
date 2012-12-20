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

import java.util.ArrayList;
import java.util.Iterator;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.messenger.Messenger;
import org.apache.qpid.proton.messenger.Status;
import org.apache.qpid.proton.messenger.Tracker;
import org.apache.qpid.proton.amqp.messaging.Accepted;
import org.apache.qpid.proton.amqp.messaging.Rejected;
import org.apache.qpid.proton.amqp.transport.DeliveryState;

class TrackerQueue
{
    private static final Accepted ACCEPTED = Accepted.getInstance();
    private static final Rejected REJECTED = new Rejected();
    private int _window = 0;
    private int _hwm = 0;
    private int _lwm = 0;
    private ArrayList<Delivery> _deliveries = new ArrayList<Delivery>();

    void setWindow(int window)
    {
        _window = window;
    }

    int getWindow()
    {
        return _window;
    }

    void accept(Tracker tracker)
    {
        accept(tracker, 0);
    }

    void accept(Tracker tracker, int flags)
    {
        apply(tracker, flags, ACCEPT);
    }

    void reject(Tracker tracker, int flags)
    {
        apply(tracker, flags, REJECT);
    }

    void settle(Tracker tracker, int flags)
    {
        apply(tracker, flags, SETTLE);
    }

    void add(Delivery delivery)
    {
        if (delivery == null)
        {
            throw new NullPointerException("Cannot add null delivery!");
        }
        int sequence = _hwm++;
        _deliveries.add(delivery);
        slide();
    }

    Status getStatus(Tracker tracker)
    {
        Delivery delivery = getDelivery(tracker);
        if (delivery != null)
        {
            DeliveryState state = delivery.getRemoteState();
            if (state != null)
            {
                return getStatus(state);
            }
            else if (delivery.remotelySettled() || delivery.isSettled())
            {
                return getStatus(delivery.getLocalState());
            }
            else
            {
                return Status.PENDING;
            }
        }
        else
        {
            return Status.UNKNOWN;
        }


    }

    private Status getStatus(DeliveryState state)
    {
        if (state instanceof Accepted)
        {
            return Status.ACCEPTED;
        }
        else if (state instanceof Rejected)
        {
            return Status.REJECTED;
        }
        else if (state == null)
        {
            return Status.PENDING;
        }
        else
        {
            throw new RuntimeException("Unexpected disposition: " + state);
        }
    }

    void slide()
    {
        if (_window >= 0)
        {
            while (_hwm - _lwm > _window)
            {
                if (_deliveries.isEmpty())
                {
                    throw new RuntimeException("Inconsistent state, empty delivery queue but lwm=" + _lwm + " and hwm=" + _hwm);
                }
                Delivery d = _deliveries.get(0);
                if (d.getLocalState() == null)
                {
                    d.disposition(ACCEPTED);
                }

                d.settle();
                _deliveries.remove(0);
                _lwm++;
            }
        }
    }

    int getHighWaterMark()
    {
        return _hwm;
    }

    Iterator<Delivery> deliveries()
    {
        return _deliveries.iterator();
    }

    private Delivery getDelivery(Tracker tracker)
    {
        int seq = ((TrackerImpl) tracker).getSequence();
        if (seq < _lwm || seq > _hwm) return null;
        int index = seq - _lwm;
        return index < _deliveries.size() ? _deliveries.get(index) : null;
    }

    static boolean isOutgoing(Tracker tracker)
    {
        return ((TrackerImpl) tracker).isOutgoing();
    }

    private void apply(Tracker tracker, int flags, DeliveryOperation operation)
    {
        int seq = ((TrackerImpl) tracker).getSequence();
        if (seq < _lwm || seq > _hwm) return;
        int last = seq - _lwm;
        int start = (flags & Messenger.CUMULATIVE) != 0 ? 0 : last;
        for (int i = start; i <= last && i < _deliveries.size(); ++i)
        {
            Delivery d = _deliveries.get(i);
            if (d != null && !d.isSettled())
            {
                operation.apply(d);
            }
        }
    }

    private static interface DeliveryOperation
    {
        void apply(Delivery d);
    }

    private static final DeliveryOperation ACCEPT = new DeliveryOperation()
        {
            public void apply(Delivery d)
            {
                if (d.getLocalState() == null) d.disposition(ACCEPTED);
            }
        };
    private static final DeliveryOperation REJECT = new DeliveryOperation()
        {
            public void apply(Delivery d)
            {
                if (d.getLocalState() == null) d.disposition(REJECTED);
            }
        };
    private static final DeliveryOperation SETTLE = new DeliveryOperation()
        {
            public void apply(Delivery d)
            {
                if (d.getLocalState() == null)
                {
                    d.disposition(d.getRemoteState());
                }
                d.settle();
            }
        };
}
