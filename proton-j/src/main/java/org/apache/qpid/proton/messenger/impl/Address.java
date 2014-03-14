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


/**
 * Address
 *
 */

class Address
{

    private String _address;
    private boolean _passive;
    private String _scheme;
    private String _user;
    private String _pass;
    private String _host;
    private String _port;
    private String _name;

    public Address(String address)
    {
        _address = address;
        parse();
    }

    private void parse()
    {
        _passive = false;
        _scheme = null;
        _user = null;
        _pass = null;
        _host = null;
        _port = null;
        _name = null;

        int start = 0;
        int schemeEnd = _address.indexOf("://", start);
        if (schemeEnd >= 0) {
            _scheme = _address.substring(start, schemeEnd);
            start = schemeEnd + 3;
        }

        String uphp;
        int slash = _address.indexOf("/", start);
        if (slash > 0) {
            uphp = _address.substring(start, slash);
            _name = _address.substring(slash + 1);
        } else {
            uphp = _address.substring(start);
        }

        String hp;
        int at = uphp.indexOf('@');
        if (at >= 0) {
            String up = uphp.substring(0, at);
            hp = uphp.substring(at + 1);

            int colon = up.indexOf(':');
            if (colon >= 0) {
                _user = up.substring(0, colon);
                _pass = up.substring(colon + 1);
            } else {
                _user = up;
            }
        } else {
            hp = uphp;
        }

        int colon = hp.indexOf(':');
        if (colon >= 0) {
            _host = hp.substring(0, colon);
            _port = hp.substring(colon + 1);
        } else {
            _host = hp;
        }

        if (_host.startsWith("~")) {
            _host = _host.substring(1);
            _passive = true;
        }
    }

    public String toString()
    {
        return _address;
    }

    public boolean isPassive()
    {
        return _passive;
    }

    public String getScheme()
    {
        return _scheme;
    }

    public String getUser()
    {
        return _user;
    }

    public String getPass()
    {
        return _pass;
    }

    public String getHost()
    {
        return _host;
    }

    public String getPort()
    {
        return _port;
    }

    public String getImpliedPort()
    {
        if (_port == null) {
            return getDefaultPort();
        } else {
            return getPort();
        }
    }

    public String getDefaultPort()
    {
        if ("amqps".equals(_scheme)) return "5671";
        else return "5672";
    }

    public String getName()
    {
        return _name;
    }

}
