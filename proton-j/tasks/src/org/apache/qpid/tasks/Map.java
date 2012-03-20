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
package org.apache.qpid.tasks;

import org.apache.tools.ant.BuildException;
import org.apache.tools.ant.util.ChainedMapper;
import org.apache.tools.ant.util.FileNameMapper;

/** Map -- an ant task that allows arbitrary use of FileNameMappers */

public class Map extends BaseTask {

    private String property;
    private String value;
    private String split = "\\s+";
    private String join = " ";
    private boolean setonempty = true;
    private ChainedMapper mapper = new ChainedMapper();

    public void setProperty(String p) {
        property = p;
    }

    public void setValue(String v) {
        value = v;
    }

    public void setSplit(String s) {
        split = s;
    }

    public void setJoin(String j) {
        join = j;
    }

    public void setSetonempty(boolean b) {
        setonempty = b;
    }

    public void add(FileNameMapper m) {
        mapper.add(m);
    }

    public void execute() {
        validate("property", property).required().nonempty();
        validate("value", value).required();

        if (mapper.getMappers().size() == 0) {
            throw new BuildException("at least one mapper must is required");
        }

        String[] parts = value.split(split);
        StringBuffer buf = new StringBuffer();
        for (int i = 0; i < parts.length; i++)
        {
            if (parts[i].length() == 0)
            {
                continue;
            }
            String[] names = mapper.mapFileName(parts[i]);

            //Mappers can return null.
            if (names != null)
            {
                for (int j = 0; j < names.length; j++)
                {
                    if (buf.length() > 0)
                    {
                        buf.append(join);
                    }
                    buf.append(names[j]);
                }
            }
        }

        if (buf.length() > 0 || setonempty) {
            getProject().setNewProperty(property, buf.toString());
        }
    }

}
