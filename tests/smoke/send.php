#!/usr/bin/env php
<?php

include("proton.php");

$messenger = new Messenger();
$messenger->outgoing_window = 10;
$message = new Message();

$address = $argv[1];
if (!$address) {
  $address = "0.0.0.0";
}

$message->address = $address;
$message->properties = Array("binding" => "php",
                             "version" => phpversion());
$message->body = "Hello World!";

$messenger->start();
$tracker = $messenger->put($message);
print "Put: $message\n";
$messenger->send();
print "Status: " . $messenger->status($tracker) . "\n";
$messenger->stop();

?>