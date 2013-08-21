#!/usr/bin/env perl
require 'qpid_proton.pm';

my $messenger = qpid::proton::Messenger->new();
$messenger->set_incoming_window(1);
my $message = qpid::proton::Message->new();

my $address = $ARGV[0];
$address = "~0.0.0.0" if !defined $address;
$messenger->subscribe($address);

$messenger->start();

while (true) {
    my $err = $messenger->receive();
    # XXX: no exceptions!
    die $messenger->get_error() if $err < 0;
    $messenger->get($message);
    print "Got: $message\n";
    $messenger->accept();
}

$messenger->stop();
