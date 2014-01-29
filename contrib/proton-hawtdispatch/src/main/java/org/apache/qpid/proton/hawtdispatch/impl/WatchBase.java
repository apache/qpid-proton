/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.qpid.proton.hawtdispatch.impl;

import org.fusesource.hawtdispatch.Dispatch;
import org.fusesource.hawtdispatch.Task;

import java.util.LinkedList;

/**
 * @author <a href="http://hiramchirino.com">Hiram Chirino</a>
 */
abstract public class WatchBase {

    private LinkedList<Watch> watches = new LinkedList<Watch>();
    protected void addWatch(final Watch task) {
        watches.add(task);
        fireWatches();
    }

    protected void fireWatches() {
        if( !this.watches.isEmpty() ) {
            Dispatch.getCurrentQueue().execute(new Task(){
                @Override
                public void run() {
                    // Lets see if any of the watches are triggered.
                    LinkedList<Watch> tmp = watches;
                    watches = new LinkedList<Watch>();
                    for (Watch task : tmp) {
                        if( !task.execute() ) {
                            watches.add(task);
                        }
                    }
                }
            });
        }
    }

}
