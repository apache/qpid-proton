<?php

include("proton.php");

function round_trip($body) {
  $msg = new Message();
  $msg->inferred = true;
  $msg->durable = true;
  $msg->id = 10;
  $msg->correlation_id = "asdf";
  $msg->properties = array();
  $msg->properties["null"] = null;
  $msg->properties["boolean"] = true;
  $msg->properties["integer"] = 123;
  $msg->properties["double"] = 3.14159;
  $msg->properties["binary"] = new Binary("binary");
  $msg->properties["symbol"] = new Symbol("symbol");
  $msg->properties["uuid"] = new UUID("1234123412341234");
  $msg->properties["list"] = new PList(1, 2, 3, 4);
  $msg->body = $body;
  assert($msg->id == 10);
  assert($msg->correlation_id == "asdf");

  $copy = new Message();
  $copy->decode($msg->encode());
  assert($copy->id == $msg->id);
  assert($copy->correlation_id == $msg->correlation_id);
  $diff = array_diff($msg->properties, $copy->properties);
  assert($copy->durable == $msg->durable);
  assert(count($diff) == 0);
  assert($copy->body == $msg->body);
}

round_trip("this is a string body");
round_trip(new Binary("this is a binary body"));
round_trip(new Symbol("this is a symbol body"));
round_trip(true);
round_trip(1234);
round_trip(3.14159);
round_trip(array("pi" => 3.14159, "blueberry-pi" => "yummy"));

?>
