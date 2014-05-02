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
 * This class provides a placeholder to cache size calculations through ThreadLocals.
 *
 * In some decoders we pre-calculate the size, and reuse the same calculation at a later point when actually writing
 * the data
 */
public class CachedCalculation
 {
     private Object _val;
     private TypeEncoding _underlyingEncoder;
     private int _size;

     public CachedCalculation setValue(Object val, TypeEncoding underlyingEncoder, int size)
     {
         this._val = val;
         this._underlyingEncoder = underlyingEncoder;
         this._size = size;
         return this;
     }

     public CachedCalculation setValue(Object val, int size)
     {
         return setValue(val, null, size);
     }

     public Object getVal()
     {
         return _val;
     }

     public TypeEncoding getUnderlyingEncoder()
     {
         return _underlyingEncoder;
     }

     public int getSize()
     {
         return _size;
     }


     private static ThreadLocal<CachedCalculation> threadLocalCalculatedArraySize = new ThreadLocal<CachedCalculation>();


     public static CachedCalculation getCache()
     {
         CachedCalculation local = threadLocalCalculatedArraySize.get();
         if (local == null)
         {
             local = new CachedCalculation();
             threadLocalCalculatedArraySize.set(local);
         }

         return local;
     }

     public static CachedCalculation setCachedValue(Object val, TypeEncoding encoding, int size)
     {
         return getCache().setValue(val, encoding, size);
     }

     public static CachedCalculation setCachedValue(Object val, int size)
     {
         return getCache().setValue(val, size);
     }
}
