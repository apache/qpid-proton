<?php

include("proton.php");

$msg = new Message();
$msg->id = 10;
$msg->correlation_id = "asdf";
assert($msg->id == 10);
assert($msg->correlation_id == "asdf");

$copy = new Message();
$copy->decode($msg->encode());
assert($copy->id == $msg->id);
assert($copy->correlation_id == $msg->correlation_id);

?>