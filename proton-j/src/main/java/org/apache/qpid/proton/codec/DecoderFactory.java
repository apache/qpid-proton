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

package org.apache.qpid.proton.codec;

/**
 * Integrates factory for Decode and Encoder.
 */
public class DecoderFactory
{

    static DecoderImpl decoder = new DecoderImpl();
    static EncoderImpl encoder = new EncoderImpl(decoder);

    public DecoderFactory() {
       AMQPDefinedTypes.registerAllTypes(decoder, encoder);
    }

    private static final DecoderFactory theInstance = new DecoderFactory();

   /**
    * gets a singleton shared pair of Decode and Encoder
    * @return
    */
   public static DecoderFactory getSingleton() {
        return theInstance;
    }

   /**
    * Produces a new Decode Encoder pair to be customized.
    * @return
    */
    public static DecoderFactory newFactory() {
        return new DecoderFactory();
    }

    public final DecoderImpl getDecoder()
    {
        return decoder;
    }

    public final EncoderImpl getEncoder()
    {
        return encoder;
    }

}
