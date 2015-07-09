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

try:
    import Queue
except:
    import queue as Queue

import sqlite3
import threading

class Db(object):
    def __init__(self, db, injector):
        self.db = db
        self.injector = injector
        self.tasks = Queue.Queue()
        self.position = None
        self.pending_events = []
        self.running = True
        self.thread = threading.Thread(target=self._process)
        self.thread.daemon=True
        self.thread.start()

    def close(self):
        self.tasks.put(lambda conn: self._close())

    def reset(self):
        self.tasks.put(lambda conn: self._reset())

    def load(self, records, event=None):
        self.tasks.put(lambda conn: self._load(conn, records, event))

    def get_id(self, event):
        self.tasks.put(lambda conn: self._get_id(conn, event))

    def insert(self, id, data, event=None):
        self.tasks.put(lambda conn: self._insert(conn, id, data, event))

    def delete(self, id, event=None):
        self.tasks.put(lambda conn: self._delete(conn, id, event))

    def _reset(self, ignored=None):
        self.position = None

    def _close(self, ignored=None):
        self.running = False

    def _get_id(self, conn, event):
        cursor = conn.execute("SELECT * FROM records ORDER BY id DESC")
        row = cursor.fetchone()
        if event:
            if row:
                event.id = row['id']
            else:
                event.id = 0
            self.injector.trigger(event)

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
            self.injector.trigger(event)

    def _insert(self, conn, id, data, event):
        if id:
            conn.execute("INSERT INTO records(id, description) VALUES (?, ?)", (id, data))
        else:
            conn.execute("INSERT INTO records(description) VALUES (?)", (data,))
        if event:
            self.pending_events.append(event)

    def _delete(self, conn, id, event):
        conn.execute("DELETE FROM records WHERE id=?", (id,))
        if event:
            self.pending_events.append(event)

    def _process(self):
        conn = sqlite3.connect(self.db)
        conn.row_factory = sqlite3.Row
        with conn:
            while self.running:
                f = self.tasks.get(True)
                try:
                    while True:
                        f(conn)
                        f = self.tasks.get(False)
                except Queue.Empty: pass
                conn.commit()
                for event in self.pending_events:
                    self.injector.trigger(event)
                self.pending_events = []
        self.injector.close()
