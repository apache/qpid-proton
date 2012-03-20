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
package org.apache.qpid.tasks;

import org.apache.tools.ant.taskdefs.Ant;

import java.io.File;
import java.util.HashSet;
import java.util.Set;

/**
 * Require
 *
 * @author Rafael H. Schloming &lt;rhs@mit.edu&gt;
 **/

public class Require extends BaseTask {

    private File file;
    private String target = "";
    private Ant ant = null;
    private String key = "";

    public void setFile(File f) {
        file = f;
    }

    public void setTarget(String t) {
        target = t;
    }

    public void setKey(String k) {
        key = k;
    }

    public void execute() {
        validate("file", file).required();

        String path = file.getAbsolutePath();
        String hash = Require.class.getName() + ":" +
            path + ":" + target + ":" + key;

        synchronized (System.class) {
            if (System.getProperty(hash) != null) {
                return;
            }

            Ant ant = (Ant) getProject().createTask("ant");
            ant.setInheritAll(false);
            ant.setOwningTarget(getOwningTarget());
            ant.setTaskName(getTaskName());
            ant.init();
            if (target.length() > 0) {
                ant.setTarget(target);
            }
            ant.setAntfile(path);
            ant.setDir(file.getParentFile());
            ant.execute();

            System.setProperty(hash, "done");
        }
    }

}
