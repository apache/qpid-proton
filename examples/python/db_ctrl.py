#!/usr/bin/env python
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

import sqlite3
import sys

if len(sys.argv) < 3:
    print "Usage: %s [init|insert|list] db" % sys.argv[0]
else:
    conn = sqlite3.connect(sys.argv[2])
    with conn:
        if sys.argv[1] == "init":
            conn.execute("DROP TABLE IF EXISTS records")
            conn.execute("CREATE TABLE records(id INTEGER PRIMARY KEY AUTOINCREMENT, description TEXT)")
            conn.commit()
        elif sys.argv[1] == "list":
            cursor = conn.cursor()
            cursor.execute("SELECT * FROM records")
            rows = cursor.fetchall()
            for r in rows:
                print r
        elif sys.argv[1] == "insert":
            while True:
                l = sys.stdin.readline()
                if not l: break
                conn.execute("INSERT INTO records(description) VALUES (?)", (l.rstrip(),))
            conn.commit()
        else:
            print "Unrecognised command: %s" %  sys.argv[1]
