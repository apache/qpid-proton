#!/usr/bin/env perl
require 'qpid_proton.pm';
my $messenger = qpid::proton::Messenger->new();
$messenger->set_outgoing_window(10);
my $message = qpid::proton::Message->new();

my $address = $ARGV[0];
$address = "0.0.0.0" if !defined $address;
$message->set_address($address);
# how do we set properties and body?

$messenger->start();
my $tracker = $messenger->put($message);
print "Put: $message\n";
$messenger->send();
print "Status: ", $messenger->status($tracker), "\n";
$messenger->stop();
