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

$counter = 1;

$sent = 0;
$rcvd = 0;

$handler = function($c) {
  $sasl = pn_connector_sasl($c);

  // check the sasl state machine and handle any authentication work
  // required
  while (pn_sasl_state($sasl) != PN_SASL_PASS) {
    switch (pn_sasl_state($sasl)) {
    case PN_SASL_CONF:
      // this state means we need to configure the sasl machinery
      // tell sasl the mechanisms we support
      pn_sasl_mechanisms($sasl, "ANONYMOUS");
      // tell sasl we're acting as the server
      pn_sasl_server($sasl);
      break;
    case PN_SASL_STEP:
      // there is authentication data to process, if we supported
      // anything other than anonymous we might read the data and send
      // a challenge
      $mech = pn_sasl_remote_mechanisms($sasl);
      if ($mech == "ANONYMOUS") {
        pn_sasl_done($sasl, PN_SASL_OK);
        pn_connector_set_connection($c, pn_connection());
      } else {
        pn_sasl_done($sasl, PN_SASL_AUTH);
      }
      break;
    case PN_SASL_FAIL:
    case PN_SASL_IDLE:
      return;
    case PN_SASL_PASS:
      break;
    }
  }

  global $counter;
  global $sent;
  global $rcvd;

  // we are authenticated (all be it anonymously)
  $conn = pn_connector_connection($c);

  // setup the connection if it's new
  $cstate = pn_connection_state($conn);
  if ($cstate & PN_LOCAL_UNINIT) {
    pn_connection_open($conn);
  }

  // setup any new sessions
  $ssn = pn_session_head($conn, PN_LOCAL_UNINIT);
  while ($ssn) {
    pn_session_open($ssn);
    $ssn = pn_session_next($ssn, PN_LOCAL_UNINIT);
  }

  // setup any new links
  $lnk = pn_link_head($conn, PN_LOCAL_UNINIT);
  while ($lnk) {
    $tgt = pn_remote_target($lnk);
    $src = pn_remote_source($lnk);
    if (pn_is_sender($lnk))
      print("Outgoing Link: $src -> $tgt\n");
    else
      print("Incoming Link: $tgt <- $src\n");
    pn_set_target($lnk, $tgt);
    pn_set_source($lnk, $src);
    pn_link_open($lnk);

    if (pn_is_sender($lnk)) {
      pn_delivery($lnk, "delivery-$counter");
      $counter++;
    } else {
      pn_flow($lnk, 10);
    }

    $lnk = pn_link_next($lnk, PN_LOCAL_UNINIT);
  }

  // process all deliveries on the work queue
  // deliveries might be in the work queue for three reasons
  //  - they are readable (incoming only)
  //  - they are writable (outgoing only)
  //  - their disposition is updated  (either)
  $delivery = pn_work_head($conn);
  while ($delivery) {
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
            break;
          } else {
            print("error reading message: $cd\n");
          }
        }
      }

      if (pn_credit($lnk) < 10) pn_flow($lnk, 10);
    } else if (pn_writable($delivery)) {
      // we have capacity to write, so let's send a message
      list ($cd, $msg) = pn_message_data("this is message $tag", 1024);
      $n = pn_send($lnk, $msg);
      if ($n != strlen($msg)) print("error sending message: $cd");
      if (pn_advance($lnk)) {
        print("sent $tag\n");
        pn_delivery($lnk, "delivery-$counter");
        $counter++;
      }
    }

    if (pn_updated($delivery)) {
      // the disposition was updated, let's report it and settle the delivery
      print("disposition for $tag: " . pn_remote_disposition($delivery) . "\n");
      // we could clear the updated flag if we didn't want to settle
      // pn_clear($delivery);
      pn_settle($delivery);
    }

    $delivery = pn_work_next($delivery);
  }

  // teardown any terminating links
  $lnk = pn_link_head($conn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
  while ($lnk) {
    pn_link_close($lnk);
    $lnk = pn_link_next($lnk, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
  }

  // teardown any terminating sessions
  $ssn = pn_session_head($conn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
  while ($ssn) {
    pn_session_close($ssn);
    $ssn = pn_session_next($ssn, PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED);
  }

  // teardown the connection if it's terminating
  if ($cstate == (PN_LOCAL_ACTIVE | PN_REMOTE_CLOSED))
    pn_connection_close($conn);
};

$driver = pn_driver();
$listener = pn_listener($driver, "0.0.0.0", "5672", NULL);
if (!$listener) {
  print("listener failed\n");
  return;
}

while (TRUE) {
  // wait forever until there is an active connector or listener
  pn_driver_wait($driver, -1);

  // cycle through all listeners with I/O activity
  while ($l = pn_driver_listener($driver)) {
    $c = pn_listener_accept($l);
    pn_connector_set_context($c, $handler);
  }

  // cycle through all connectors with I/O activity
  while ($c = pn_driver_connector($driver)) {
    // process available I/O events
    pn_connector_process($c);
    // grab the handler and call it
    $h = pn_connector_context($c);
    $h($c);
    if (pn_connector_closed($c)) {
      // free the connector if closed
      pn_connection_free(pn_connector_connection($c));
      pn_connector_free($c);
      unset($c);
    } else {
      // otherwise process any work the handler might have done
      pn_connector_process($c);
    }
  }
}

?>
