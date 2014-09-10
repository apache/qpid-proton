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

import Queue
import sqlite3
import threading

class Db(object):
    def __init__(self, db, events):
        self.db = db
        self.events = events
        self.tasks = Queue.Queue()
        self.position = None
        self.thread = threading.Thread(target=self._process)
        self.thread.daemon=True
        self.thread.start()

    def reset(self):
        self.tasks.put(lambda conn: self._reset())

    def load(self, records, event=None):
        self.tasks.put(lambda conn: self._load(conn, records, event))

    def insert(self, id, data, event=None):
        self.tasks.put(lambda conn: self._insert(conn, id, data, event))

    def delete(self, id, event=None):
        self.tasks.put(lambda conn: self._delete(conn, id, event))

    def _reset(self, ignored=None):
        self.position = None

    def _load(self, conn, records, event):
        if self.position:
            cursor = conn.execute("SELECT * FROM records WHERE id > ? ORDER BY id", (self.position,))
        else:
            cursor = conn.execute("SELECT * FROM records ORDER BY id")
        while not records.full():
            row = cursor.fetchone()
            if row:
                self.position = row['id']
                records.put(dict(row))
            else:
                break
        if event:
            self.events.trigger(event)

    def _insert(self, conn, id, data, event):
        if id:
            conn.execute("INSERT INTO records(id, description) VALUES (?, ?)", (id, data))
        else:
            conn.execute("INSERT INTO records(description) VALUES (?)", (data,))
        conn.commit()
        if event:
            self.events.trigger(event)

    def _delete(self, conn, id, event):
        conn.execute("DELETE FROM records WHERE id=?", (id,))
        conn.commit()
        if event:
            self.events.trigger(event)

    def _process(self):
        conn = sqlite3.connect(self.db)
        conn.row_factory = sqlite3.Row
        with conn:
            while True:
                f = self.tasks.get(True)
                f(conn)
