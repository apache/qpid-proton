<?php

/**
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
 **/

include("proton.php");

$mess = new Messenger();
$mess->start();

if ($argv[1]) {
  $mess->subscribe($argv[1]);
} else {
  $mess->subscribe("amqp://~0.0.0.0");
}

$msg = new Message();
while (true) {
  $mess->recv(10);
  while ($mess->incoming) {
    try {
      $mess->get($msg);
    } catch (Exception $e) {
      print "$e\n";
      continue;
    }

    print "$msg->address, $msg->subject, $msg->body\n";
  }
}

$mess->stop();
?>
