/*
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
 */
package org.apache.qpid.proton.logging;

import java.util.Date;

public class StdOutCategoryLogger implements ProtonCategoryLogger
{

    @Override
    public boolean isEnabled(String category, ProtonLogLevel level)
    {
        return true;
    }

    @Override
    public void log(String category, ProtonLogLevel level, String message)
    {
        System.out.println(String.format("%1$tF %1$tT.%tL [%s] [%s] %s",
                                         new Date(), level, category, message));
    }
}
