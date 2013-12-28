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
package org.apache.qpid.proton.engine.jni;

import org.apache.qpid.proton.ProtonCEquivalent;
import org.apache.qpid.proton.engine.Delivery;
import org.apache.qpid.proton.engine.Link;
import org.apache.qpid.proton.jni.Proton;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_delivery_t;
import org.apache.qpid.proton.jni.SWIGTYPE_p_pn_disposition_t;
import org.apache.qpid.proton.amqp.messaging.Accepted;
import org.apache.qpid.proton.amqp.messaging.Modified;
import org.apache.qpid.proton.amqp.messaging.Received;
import org.apache.qpid.proton.amqp.messaging.Rejected;
import org.apache.qpid.proton.amqp.messaging.Released;
import org.apache.qpid.proton.amqp.transport.DeliveryState;

import static org.apache.qpid.proton.jni.ProtonConstants.*;
import java.math.BigInteger;

public class JNIDelivery implements Delivery
{
    private SWIGTYPE_p_pn_delivery_t _impl;
    private Object _context;
    private JNILink _link;

    /**
     *
     * Note that we give the c layer a reference to the Java representation of the delivery
     * so we can return the same Delivery object to the application throughput its lifetime.
     * Used by {@link #getDelivery(SWIGTYPE_p_pn_delivery_t)}
     */
    public JNIDelivery(SWIGTYPE_p_pn_delivery_t delivery_t)
    {
        _impl = delivery_t;
                Proton.pn_delivery_set_context(_impl, this);
        _link = JNILink.getLink(Proton.pn_delivery_link(_impl));
    }

    static Delivery getDelivery(SWIGTYPE_p_pn_delivery_t delivery_t)
    {
        if(delivery_t != null)
        {
            Delivery deliveryObj = (Delivery) Proton.pn_delivery_get_context(delivery_t);
            if(deliveryObj == null)
            {
                deliveryObj = new JNIDelivery(delivery_t);
            }
            return deliveryObj;
        }
        return null;
    }

    @ProtonCEquivalent("pn_delivery_get_context")
    public Object getContext()
    {
        return _context;
    }

    @Override
    @ProtonCEquivalent("pn_delivery_tag")
    public byte[] getTag()
    {
        // TODO - pn_delivery_tag_t should be bytes not string
        return Proton.pn_delivery_tag(_impl);

    }

    @Override
    @ProtonCEquivalent("pn_delivery_link")
    public Link getLink()
    {
        return _link;
    }

    @Override
    @ProtonCEquivalent("pn_delivery_local_state")
    public DeliveryState getLocalState()
    {
        return convertDisposition(Proton.pn_delivery_local_state(_impl));
    }

    @Override
    @ProtonCEquivalent("pn_delivery_remote_state")
    public DeliveryState getRemoteState()
    {
        return convertDisposition(Proton.pn_delivery_remote_state(_impl));
    }

    @Override
    public boolean remotelySettled()
    {
        return Proton.pn_delivery_settled(_impl);
    }

    @Override
    public int getMessageFormat()
    {
        return 0;  //TODO
    }

    @Override
    public void disposition(DeliveryState state)
    {
        Proton.pn_delivery_update(_impl, convertState(state));
        //TODO
    }

    private static BigInteger convertState(DeliveryState state)
    {
        //TODO - disposition properties conversion
        if(state instanceof Accepted)
        {
            return BigInteger.valueOf(PN_ACCEPTED);
        }
        else if(state instanceof Rejected)
        {
            return BigInteger.valueOf(PN_REJECTED);
        }
        else if(state instanceof Modified)
        {
            return BigInteger.valueOf(PN_MODIFIED);
        }
        else if(state instanceof Received)
        {
            return BigInteger.valueOf(PN_RECEIVED);
        }
        else if(state instanceof Released)
        {
            return BigInteger.valueOf(PN_RELEASED);
        }

        return BigInteger.ZERO;
    }

    private static DeliveryState convertDisposition(BigInteger disposition)
    {
        //TODO - disposition properties conversion
        if (BigInteger.valueOf(PN_ACCEPTED).equals(disposition))
        {
            return Accepted.getInstance();
        }
        else if(BigInteger.valueOf(PN_REJECTED).equals(disposition))
        {
            return new Rejected();
        }
        else if(BigInteger.valueOf(PN_MODIFIED).equals(disposition))
        {
            return new Modified();
        }
        else if(BigInteger.valueOf(PN_RECEIVED).equals(disposition))
        {
            return new Received();
        }
        else if(BigInteger.valueOf(PN_RELEASED).equals(disposition))
        {
            return new Released();
        }

        return null;
    }

    @Override
    @ProtonCEquivalent("pn_delivery_settle")
    public void settle()
    {
        Proton.pn_delivery_settle(_impl);
    }

    @Override
    public void free()
    {
        if(_impl != null)
        {
            Proton.pn_delivery_set_context(_impl, null);
//            Proton.pn_delivery_free(_impl);
            _impl = null;
        }
    }

    @Override
    public Delivery getWorkNext()
    {
        return getDelivery(Proton.pn_work_next(_impl));
    }

    @Override
    @ProtonCEquivalent("pn_delivery_writable")
    public boolean isWritable()
    {
        return Proton.pn_delivery_writable(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_delivery_readable")
    public boolean isReadable()
    {
        return Proton.pn_delivery_readable(_impl);
    }

    @ProtonCEquivalent("pn_delivery_set_context")
    public void setContext(Object context)
    {
        _context = context;
    }

    @Override
    @ProtonCEquivalent("pn_delivery_updated")
    public boolean isUpdated()
    {
        return Proton.pn_delivery_updated(_impl);
    }

    @Override
    public void clear()
    {
        Proton.pn_delivery_clear(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_delivery_partial")
    public boolean isPartial()
    {
        return Proton.pn_delivery_partial(_impl);
    }

    @Override
    @ProtonCEquivalent("pn_delivery_settled")
    public boolean isSettled()
    {
        return Proton.pn_delivery_settled(_impl);
    }

    @Override
    protected void finalize() throws Throwable
    {
        // TODO if the delivery is not settled, surely it never gets free'd, thereby leaking memory
        if(isSettled())
        {
            free();
        }
        super.finalize();
    }

    @ProtonCEquivalent("pn_delivery_pending")
    public int pending()
    {
        return (int) Proton.pn_delivery_pending(_impl);
    }

    @ProtonCEquivalent("pn_delivery_buffered")
    public boolean isBuffered()
    {
        return Proton.pn_delivery_buffered(_impl);
    }
}
