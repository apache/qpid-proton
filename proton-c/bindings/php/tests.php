<?php
  // run from the command line lke this:
  // php -d extension=<path-to>/php/libcproton.so -f <path-to>/php/tests.php
  //
include("cproton.php");

function pump($t1, $t2) {
    while (TRUE) {
        $out1 = pn_output($t1, 1024);
        $out2 = pn_output($t2, 1024);

        assert("$out1[0] >= 0");
        assert("$out2[0] >= 0");
        //print("pn_output1[0]= " . $out1[0] . " [1]=" . $out1[1] . "\n");
        //print("pn_output2[0]= " . $out2[0] . " [1]=" . $out2[1] . "\n");

        if ($out1[1] or $out2[1]) {
            if ($out1[1]) {
                $cd = pn_input($t2, $out1[1]);
                print("pn_input1= " . $cd . "\n");
                assert('$cd == strlen($out1[1])');
            }
            if ($out2[1]) {
                $cd = pn_input($t1, $out2[1]);
                print("pn_input2= " . $cd . "\n");
                assert('$cd == strlen($out2[1])');
            }
        } else {
            return;
        }
    }
}

print("BEGIN TEST\n");

// class Test setup
$c1 = pn_connection();
$t1 = pn_transport($c1);
pn_transport_open($t1);

$c2 = pn_connection();
$t2 = pn_transport($c2);
pn_transport_open($t2);


// class TransferTest setup
$ssn1 = pn_session($c1);
pn_connection_open($c1);
pn_connection_open($c2);
pn_session_open($ssn1);
pump($t1, $t2);
$ssn2 = pn_session_head($c2, PN_LOCAL_UNINIT | PN_REMOTE_ACTIVE);
pn_session_open($ssn2);

$snd = pn_sender($ssn1, "test-link");
$rcv = pn_receiver($ssn2, "test-link");

pn_link_open($snd);
pn_link_open($rcv);
pump($t1, $t2);

// test_disposition

pn_flow($rcv, 1);

pump($t1, $t2);

$sd = pn_delivery($snd, "tag");
$xxx = pn_delivery_tag($sd);
assert('$xxx == "tag"');
//print("Delivery Tag=" . $xxx . "\n");


$msg = "this is a test";
$n = pn_send($snd, $msg);
//print("pn_send= " . $n . "\n");
assert('$n == 14');
pn_advance($snd);

pump($t1, $t2);


$rd = pn_current($rcv);
assert('pn_delivery_tag($rd) == "tag"');
assert('pn_delivery_tag($sd) == "tag"');
//print("pn_delivery_tag= " .  . "\n");
//print("pn_delivery_tag= " . pn_delivery_tag($sd) . "\n");

$rmsg = pn_recv($rcv, 1024);
//print("pn_recv [0]= " . $rmsg[0] . " [1]= " . $rmsg[1] . "\n" );
assert('$rmsg[0] == 14');
assert('$rmsg[1] == "this is a test"');
pn_disposition($rd, PN_ACCEPTED);

pump($t1, $t2);

//    assert pn_remote_disposition(sd) == pn_local_disposition(rd) == PN_ACCEPTED
//    assert pn_updated(sd)

pn_disposition($sd, PN_ACCEPTED);
pn_settle($sd);

pump($t1, $t2);

// cleanup
pn_connection_free($c1);
pn_connection_free($c2);


//
// pn_message_data test
//

$x = pn_message_data("This is test data!", 1024);
//print("Msg Len  = " . $x[0] . "\n");
assert("$x[0] > 0");
//print("Msg data = " . $x[1] . "\n");


//
// pn_listener/pn_connector context handling
//

$pnd = pn_driver();

// test empty listener & connector
$pnl = pn_driver_listener($pnd);
$pnlc = pn_listener_context($pnl);
//print("Empty listener " . $pnlc . "\n");
unset($pnlc);
pn_listener_free($pnl);

$pnc = pn_driver_connector($pnd);
$pncc = pn_connector_context($pnc);
//print("Empty connector " . $pncc . "\n");
//assert("is_null($pncc)");
unset($pncc);

pn_connector_free($pnc);


// manage a listener context
$x = "listener-context";
$pnl = pn_listener_fd($pnd, 1, $x);
unset($x);
$x = pn_listener_context($pnl);
assert('$x == "listener-context"');
//print("Retrieved listener context: " . $x . "\n");
unset($x);
$m = pn_listener_context($pnl);
pn_listener_free($pnl);
//print("After free: " . $m . "\n");
assert('$m == "listener-context"');


// manage a connector context
$y = "connector-context";
$pnc = pn_connector_fd($pnd, 2, $y);
unset($y);
$y = pn_connector_context($pnc);
//print("Retrieved connector context: " . $y . "\n");
assert('$y == "connector-context"');
$y = 75;
pn_connector_set_context($pnc, $y);
unset($y);
//print("Updated connector context: " . pn_connector_context($pnc) . "\n");
assert('pn_connector_context($pnc) == 75');
$m = pn_connector_context($pnc);
pn_connector_free($pnc);
//print("After free: " . $m . "\n");
assert('$m == 75');


pn_driver_free($pnd);


print("END TEST\n");

?>