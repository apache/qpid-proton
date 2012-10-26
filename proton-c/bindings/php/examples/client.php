<?php
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

  // run from the command line like this:
  // php -d extension=<path-to>/php/libcproton.so test.php
  //
include("cproton.php");

gc_enable();

// defaults to anonymous for NULL
$user = NULL;
$pass = NULL;

// send and recv this many messages (0 means infinite)
$count = 10;

$counter = 1;
$sent = 0;
$rcvd = 0;

$handler = function($c) {
  $sasl = pn_connector_sasl($c);
  switch (pn_sasl_state($sasl)) {
    case PN_SASL_CONF:
    case PN_SASL_STEP:
    case PN_SASL_IDLE:
    case PN_SASL_FAIL:
      return;
    case PN_SASL_PASS:
      break;
  }

  global $count, $counter, $sent, $rcvd;

  $conn = pn_connector_connection($c);

  // our setup was done up front, so just process the work queue
  $delivery = pn_work_head($conn);
  while ($delivery)
  {
    $lnk = pn_link($delivery);
    $tag = pn_delivery_tag($delivery);

    if (pn_readable($delivery)) {
      // read until the end of the message
      while (TRUE) {
        list ($cd, $msg) = pn_recv($lnk, 1024);
        if ($msg) print("message: $tag\n");
        if ($cd < 0) {
          if ($cd == PN_EOS) {
            // now that we hit the end of the message, updated the
            // disposition and advance the link to the next message
            pn_disposition($delivery, PN_ACCEPTED);
            pn_advance($lnk);
            $rcvd++;
            break;
          } else {
            print("error reading message: $cd\n");
          }
        }
      }

      $delta = min($count ? $count : 10 - $rcvd, 10);
      if ($delta && pn_credit($lnk) < $delta) {
        pn_flow($lnk, $delta);
      }

      if ($count && $rcvd == $count) {
        pn_link_close($lnk);
      }
    } else if (pn_writable($delivery)) {
      // we have capacity to write, so let's send a message
      list ($cd, $msg) = pn_message_data("this is message $tag", 1024);
      $n = pn_send($lnk, $msg);
      if ($n != strlen($msg)) print("error sending message: $cd");
      if (pn_advance($lnk)) {
        print("sent $tag\n");
        $sent++;
        if (!$count || $sent < $count) {
          pn_delivery($lnk, "delivery-$counter");
          $counter++;
        } else {
          pn_link_close($lnk);
        }
      }
    }

    if (pn_updated($delivery)) {
      // the disposition was updated, let's report it and settle the delivery
      print "disposition for $tag: " .
        pn_local_disposition($delivery) . " " .
        pn_remote_disposition($delivery) . "\n";
      // we could clear the updated flag if we didn't want to settle
      // pn_clear($delivery);
      pn_settle($delivery);
    }

    $delivery = pn_work_next($delivery);
  }

  if ($count && $sent == $count && $rcvd == $count)
    pn_connection_close($conn);
};

$driver = pn_driver();
$c = pn_connector($driver, "0.0.0.0", "5672", $handler);
if (!$c) {
  print("connect failed\n");
  return;
}

// configure for client sasl
$sasl = pn_connector_sasl($c);
if ($user) {
  pn_sasl_plain($sasl, $user, $pass);
} else {
  pn_sasl_mechanisms($sasl, "ANONYMOUS");
  pn_sasl_client($sasl);
}

// set up a session with a sender and receiver
$conn = pn_connection();
pn_connector_set_connection($c, $conn);
pn_connection_set_hostname($conn, "rschloming.servicebus.appfabriclabs.com");
pn_connection_set_container($conn, "asdf");
$ssn = pn_session($conn);
$snd = pn_sender($ssn, "sender");
pn_set_target($snd, "queue1");
$rcv = pn_receiver($ssn, "receiver");
pn_set_source($rcv, "queue1");

// open all the endpoints
pn_connection_open($conn);
pn_session_open($ssn);
pn_link_open($snd);
pn_link_open($rcv);

// set up an initial delivery
pn_delivery($snd, "delivery-$counter");
$counter++;

// allocate some initial credit
pn_flow($rcv, min($count ? $count : 10, 10));

$done = false;
while (!$done) {
  // wait until there is an active connector or listener
  pn_driver_wait($driver, -1);

  // cycle through all connectors with I/O activity
  while ($c = pn_driver_connector($driver)) {
    // process work due to I/O events
    pn_connector_process($c);
    $h = pn_connector_context($c);
    $h($c);
    if (pn_connector_closed($c)) {
      pn_connection_free(pn_connector_connection($c));
      pn_connector_free($c);
      unset($c);
      $done = true;
    } else {
      // process work due to the handler
      pn_connector_process($c);
    }
  }
}

?>
