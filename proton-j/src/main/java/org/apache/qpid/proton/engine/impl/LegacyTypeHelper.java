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

import java.util.HashMap;
import java.util.Map;

import org.apache.qpid.proton.amqp.Symbol;
import org.apache.qpid.proton.amqp.UnsignedInteger;
import org.apache.qpid.proton.amqp.UnsignedLong;
import org.apache.qpid.proton.message2.Accepted;
import org.apache.qpid.proton.message2.Modified;
import org.apache.qpid.proton.message2.Received;
import org.apache.qpid.proton.message2.Rejected;
import org.apache.qpid.proton.message2.Released;
import org.apache.qpid.proton.transport2.DeliveryState;
import org.apache.qpid.proton.transport2.ErrorCondition;
import org.apache.qpid.proton.transport2.ReceiverSettleMode;
import org.apache.qpid.proton.transport2.SenderSettleMode;
import org.apache.qpid.proton.transport2.Source;
import org.apache.qpid.proton.transport2.Target;
import org.apache.qpid.proton.transport2.TerminusDurability;
import org.apache.qpid.proton.transport2.TerminusExpiryPolicy;

/**
 * A temporary arrangement to convert from the new Type system to the old model.
 * The old system is used heavily within the Engine API layer therefore
 * dislodging it should be done as a separate step after upstream discussion.
 * 
 * This class can be removed when we remove the old type system.
 *
 */
public class LegacyTypeHelper
{
    public static Map<String, Object> convertSymbolToStringKeyMap(Map<Symbol, Object> in)
    {
        if (in == null)
        {
            return null;
        }
        Map<String, Object> out = new HashMap<String, Object>(in.size());
        for (Symbol sym : in.keySet())
        {
            out.put(sym.toString(), in.get(sym));
        }
        return out;
    }

    public static Map<Symbol, Object> convertStringToSymbolKeyMap(Map<String, Object> in)
    {
        if (in == null)
        {
            return null;
        }
        Map<Symbol, Object> out = new HashMap<Symbol, Object>(in.size());
        for (String str : in.keySet())
        {
            out.put(Symbol.valueOf(str), in.get(str));
        }
        return out;
    }

    public static Symbol[] convertToSymbolArray(String[] in)
    {
        if (in == null)
        {
            return null;
        }
        Symbol[] out = new Symbol[in.length];
        for (int i = 0; i < in.length; i++)
        {
            out[i] = Symbol.valueOf(in[i]);
        }
        return out;
    }

    public static String[] convertToStringArray(Symbol[] in)
    {
        if (in == null)
        {
            return null;
        }
        String[] out = new String[in.length];
        for (int i = 0; i < in.length; i++)
        {
            out[i] = in[i].toString();
        }
        return out;
    }

    public static org.apache.qpid.proton.amqp.transport.ErrorCondition convertToLegacyErrorCondition(
            ErrorCondition error)
    {
        org.apache.qpid.proton.amqp.transport.ErrorCondition condition = new org.apache.qpid.proton.amqp.transport.ErrorCondition();
        condition.setCondition(Symbol.valueOf(error.getCondition()));
        condition.setDescription(condition.getDescription());
        condition.setInfo(error.getInfo());
        return condition;
    }

    public static ErrorCondition convertFromLegacyErrorCondition(
            org.apache.qpid.proton.amqp.transport.ErrorCondition error)
    {
        if (error != null)
        {
            return new ErrorCondition(error.getCondition() == null ? null: error.getCondition().toString(), error.getDescription());
        }
        else
        {
            return null;
        }
    }

    public static DeliveryState convertFromLegacyDeliveryState(org.apache.qpid.proton.amqp.transport.DeliveryState state)
    {
        if (state instanceof org.apache.qpid.proton.amqp.messaging.Accepted)
        {
            return Accepted.getInstance();
        }
        if (state instanceof org.apache.qpid.proton.amqp.messaging.Rejected)
        {
            org.apache.qpid.proton.amqp.messaging.Rejected original = (org.apache.qpid.proton.amqp.messaging.Rejected) state;
            Rejected rejected = new Rejected();
            rejected.setError(convertFromLegacyErrorCondition(original.getError()));
            return rejected;
        }
        if (state instanceof org.apache.qpid.proton.amqp.messaging.Released)
        {
            return Released.getInstance();
        }
        else if (state instanceof org.apache.qpid.proton.amqp.messaging.Modified)
        {
            org.apache.qpid.proton.amqp.messaging.Modified original = (org.apache.qpid.proton.amqp.messaging.Modified) state;
            Modified modified = new Modified();
            modified.setDeliveryFailed(original.getDeliveryFailed());
            modified.setUndeliverableHere(original.getUndeliverableHere());
            modified.setMessageAnnotations(original.getMessageAnnotations());
            return modified;
        }
        else if (state instanceof org.apache.qpid.proton.amqp.messaging.Received)
        {
            org.apache.qpid.proton.amqp.messaging.Received original = (org.apache.qpid.proton.amqp.messaging.Received) state;
            Received received = new Received();
            received.setSectionNumber(original.getSectionNumber().intValue());
            received.setSectionOffset(original.getSectionOffset().longValue());
            return received;
        }
        else
        {
            return null;
        }
    }

    public static org.apache.qpid.proton.amqp.transport.DeliveryState convertToLegacyDeliveryState(DeliveryState state)
    {
        if (state instanceof Accepted)
        {
            return org.apache.qpid.proton.amqp.messaging.Accepted.getInstance();
        }
        if (state instanceof Rejected)
        {
            Rejected original = (Rejected) state;
            org.apache.qpid.proton.amqp.messaging.Rejected rejected = new org.apache.qpid.proton.amqp.messaging.Rejected();
            rejected.setError(convertToLegacyErrorCondition(original.getError()));
        }
        if (state instanceof Released)
        {
            return org.apache.qpid.proton.amqp.messaging.Released.getInstance();
        }
        else if (state instanceof Modified)
        {
            Modified original = (Modified) state;
            org.apache.qpid.proton.amqp.messaging.Modified modified = new org.apache.qpid.proton.amqp.messaging.Modified();
            modified.setDeliveryFailed(original.getDeliveryFailed());
            modified.setUndeliverableHere(original.getUndeliverableHere());
            modified.setMessageAnnotations(original.getMessageAnnotations());
            return modified;
        }
        else if (state instanceof Received)
        {
            Received original = (Received) state;
            org.apache.qpid.proton.amqp.messaging.Received received = new org.apache.qpid.proton.amqp.messaging.Received();
            received.setSectionNumber(UnsignedInteger.valueOf(original.getSectionNumber()));
            received.setSectionOffset(UnsignedLong.valueOf(original.getSectionOffset()));
            return received;
        }
        else
        {
            return null;
        }
    }

    public static SenderSettleMode convertFromLegacySenderSettleMode(
            org.apache.qpid.proton.amqp.transport.SenderSettleMode senderSettleMode)
    {
        switch (senderSettleMode)
        {
        case UNSETTLED:
            return SenderSettleMode.UNSETTLED;
        case SETTLED:
            return SenderSettleMode.SETTLED;
        case MIXED:
            return SenderSettleMode.MIXED;
        default:
            throw new RuntimeException("Illegal Sender Settle Mode");

        }
    }

    public static ReceiverSettleMode convertFromLegacyReceiverSettleMode(
            org.apache.qpid.proton.amqp.transport.ReceiverSettleMode receiverSettleMode)
    {
        switch (receiverSettleMode)
        {
        case FIRST:
            return ReceiverSettleMode.FIRST;
        case SECOND:
            return ReceiverSettleMode.SECOND;
        default:
            throw new RuntimeException("Illegal Receiver Settle Mode");

        }
    }

    public static Source convertFromLegacySource(org.apache.qpid.proton.amqp.transport.Source source)
    {
        if (source == null)
        {
            return null;
        }
        else
        {
            org.apache.qpid.proton.amqp.messaging.Source legacy = (org.apache.qpid.proton.amqp.messaging.Source) source;
            Source s = new Source();
            s.setAddress(legacy.getAddress());
            s.setCapabilities(convertToStringArray(legacy.getCapabilities()));
            // s.setDefaultOutcome(legacy.getDefaultOutcome());
            s.setDistributionMode(legacy.getDistributionMode() == null ? null : legacy.getDistributionMode().toString());
            s.setDurable(TerminusDurability.get(legacy.getDurable().getValue().byteValue()));
            s.setDynamic(legacy.getDynamic());
            s.setDynamicNodeProperties(legacy.getDynamicNodeProperties());
            s.setExpiryPolicy(TerminusExpiryPolicy.getEnum(legacy.getExpiryPolicy().getPolicy().toString()));
            s.setFilter(legacy.getFilter());
            s.setOutcomes(convertToStringArray(legacy.getOutcomes()));
            s.setTimeout(legacy.getTimeout().intValue());
            return s;
        }
    }

    public static Target convertFromLegacyTarget(org.apache.qpid.proton.amqp.transport.Target target)
    {
        if (target == null)
        {
            return null;
        }
        else
        {
            org.apache.qpid.proton.amqp.messaging.Target legacy = (org.apache.qpid.proton.amqp.messaging.Target) target;
            Target t = new Target();
            t.setAddress(legacy.getAddress());
            t.setCapabilities(convertToStringArray(legacy.getCapabilities()));
    
            t.setDurable(TerminusDurability.get(legacy.getDurable().getValue().byteValue()));
            t.setDynamic(legacy.getDynamic());
            t.setDynamicNodeProperties(legacy.getDynamicNodeProperties());
            t.setExpiryPolicy(TerminusExpiryPolicy.getEnum(legacy.getExpiryPolicy().getPolicy().toString()));
            t.setTimeout(legacy.getTimeout().intValue());
            return t;
        }
    }

    public static org.apache.qpid.proton.amqp.transport.Source convertToLegacySource(Source s)
    {
        if (s == null)
        {
            return null;
        }
        else
        {
            org.apache.qpid.proton.amqp.messaging.Source legacy = new org.apache.qpid.proton.amqp.messaging.Source();
            legacy.setAddress(s.getAddress());
            legacy.setCapabilities(convertToSymbolArray(s.getCapabilities()));
            // legacy.setDefaultOutcome(s.getDefaultOutcome());
            legacy.setDistributionMode(s.getDistributionMode() == null ? null : Symbol.valueOf(s.getDistributionMode().toString()));
            legacy.setDurable(org.apache.qpid.proton.amqp.messaging.TerminusDurability.get(UnsignedInteger.valueOf(s
                    .getDurable().getValue())));
            legacy.setDynamic(s.getDynamic());
            legacy.setDynamicNodeProperties(s.getDynamicNodeProperties());
            legacy.setFilter(s.getFilter());
            legacy.setOutcomes(convertToSymbolArray(s.getOutcomes()));
            legacy.setTimeout(UnsignedInteger.valueOf(s.getTimeout()));
            return legacy;
        }
    }
    
    public static org.apache.qpid.proton.amqp.transport.Target convertToLegacyTarget(Target target)
    {
        if (target == null)
        {
            return null;
        }
        else
        {
            org.apache.qpid.proton.amqp.messaging.Target legacy = new org.apache.qpid.proton.amqp.messaging.Target();
            legacy.setAddress(target.getAddress());
            legacy.setCapabilities(convertToSymbolArray(target.getCapabilities()));
    
            legacy.setDurable(org.apache.qpid.proton.amqp.messaging.TerminusDurability.get(UnsignedInteger.valueOf((int)target.getDurable().getValue())));
            legacy.setDynamic(target.getDynamic());
            legacy.setDynamicNodeProperties(target.getDynamicNodeProperties());
            legacy.setExpiryPolicy(org.apache.qpid.proton.amqp.messaging.TerminusExpiryPolicy.valueOf(target.getExpiryPolicy().toString()));
            legacy.setTimeout(UnsignedInteger.valueOf(target.getTimeout()));
            return legacy;
        }
    }
}