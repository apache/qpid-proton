<?php

include("proton.php");

$msg = new Message();
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
$msg->properties["list"] = new Lst(1, 2, 3, 4);
assert($msg->id == 10);
assert($msg->correlation_id == "asdf");

$copy = new Message();
$copy->decode($msg->encode());
assert($copy->id == $msg->id);
assert($copy->correlation_id == $msg->correlation_id);
$diff = array_diff($msg->properties, $copy->properties);
assert(count($diff) == 0);

?>