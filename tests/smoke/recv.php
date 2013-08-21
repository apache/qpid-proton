#!/usr/bin/env php
<?php

include("proton.php");

$messenger = new Messenger();
$messenger->incoming_window = 10;
$message = new Message();

$address = $argv[1];
if (!$address) {
  $address = "~0.0.0.0";
}
$messenger->subscribe($address);

$messenger->start();

while (true) {
  $messenger->recv();
  $messenger->get($message);
  print "Got: $message\n";
  $messenger->accept();
}

$messenger->stop();

?>