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

import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;


/**
 * Transform
 *
 */

class Transform
{

    private class Rule {

        String _pattern;
        String _substitution;

        Pattern _compiled;
        StringBuilder _sb = new StringBuilder();
        boolean _matched = false;
        String _result = null;

        Rule(String pattern, String substitution)
        {
            _pattern = pattern;
            _substitution = substitution;
            _compiled = Pattern.compile(_pattern.replace("*", "(.*)").replace("%", "([^/]*)"));
        }

        boolean apply(String src) {
            _matched = false;
            _result = null;
            Matcher m = _compiled.matcher(src);
            if (m.matches()) {
                _matched = true;
                if (_substitution != null) {
                    _sb.setLength(0);
                    int limit = _substitution.length();
                    int idx = 0;
                    while (idx < limit) {
                        char c = _substitution.charAt(idx);
                        switch (c) {
                        case '$':
                            idx++;
                            if (idx < limit) {
                                c = _substitution.charAt(idx);
                            } else {
                                throw new IllegalStateException("substition index truncated");
                            }

                            if (c == '$') {
                                _sb.append(c);
                                idx++;
                            } else {
                                int num = 0;
                                while (Character.isDigit(c)) {
                                    num *= 10;
                                    num += c - '0';
                                    idx++;
                                    c = idx < limit ? _substitution.charAt(idx) : '\0';
                                }
                                if (num > 0) {
                                    _sb.append(m.group(num));
                                } else {
                                    throw new IllegalStateException
                                        ("bad substitution index at character[" +
                                         idx + "]: " + _substitution);
                                }
                            }
                            break;
                        default:
                            _sb.append(c);
                            idx++;
                            break;
                        }
                    }
                    _result = _sb.toString();
                }
            }

            return _matched;
        }

        boolean matched() {
            return _matched;
        }

        String result() {
            return _result;
        }

    }

    private List<Rule> _rules = new ArrayList<Rule>();
    private Rule _matched = null;

    public void rule(String pattern, String substitution)
    {
        _rules.add(new Rule(pattern, substitution));
    }

    public boolean apply(String src)
    {
        _matched = null;

        for (Rule rule: _rules) {
            if (rule.apply(src)) {
                _matched = rule;
                break;
            }
        }

        return _matched != null;
    }

    public boolean matched()
    {
        return _matched != null;
    }

    public String result()
    {
        return _matched != null ? _matched.result() : null;
    }

}
