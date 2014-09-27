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

public class Address
{

    private String _address;
    private boolean _passive;
    private String _scheme;
    private String _user;
    private String _pass;
    private String _host;
    private String _port;
    private String _name;

    public void clear()
    {
        _passive = false;
        _scheme = null;
        _user = null;
        _pass = null;
        _host = null;
        _port = null;
        _name = null;
    }

    public Address()
    {
        clear();
    }

    public Address(String address)
    {
        clear();
        int start = 0;
        int schemeEnd = address.indexOf("://", start);
        if (schemeEnd >= 0) {
            _scheme = address.substring(start, schemeEnd);
            start = schemeEnd + 3;
        }

        String uphp;
        int slash = address.indexOf("/", start);
        if (slash >= 0) {
            uphp = address.substring(start, slash);
            _name = address.substring(slash + 1);
        } else {
            uphp = address.substring(start);
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

        if (hp.startsWith("[")) {
            int close = hp.indexOf(']');
            if (close >= 0) {
                _host = hp.substring(1, close);
                if (hp.substring(close + 1).startsWith(":")) {
                    _port = hp.substring(close + 2);
                }
            }
        }

        if (_host == null) {
            int colon = hp.indexOf(':');
            if (colon >= 0) {
                _host = hp.substring(0, colon);
                _port = hp.substring(colon + 1);
            } else {
                _host = hp;
            }
        }

        if (_host.startsWith("~")) {
            _host = _host.substring(1);
            _passive = true;
        }
    }

    public String toString()
    {
        String  str = new String();
        if (_scheme != null) str += _scheme + "://";
        if (_user != null) str += _user;
        if (_pass != null) str += ":" + _pass;
        if (_user != null || _pass != null) str += "@";
        if (_host != null) {
            if (_host.contains(":")) str += "[" + _host + "]";
            else str += _host;
        }
        if (_port != null) str += ":" + _port;
        if (_name != null) str += "/" + _name;
        return str;
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

    public void setScheme(String scheme)
    {
        _scheme= scheme;
    }

    public void setUser(String user)
    {
        _user= user;
    }

    public void setPass(String pass)
    {
        _pass= pass;
    }

    public void setHost(String host)
    {
        _host= host;
    }

    public void setPort(String port)
    {
        _port= port;
    }

    public void setName(String name)
    {
        _name= name;
    }
}
