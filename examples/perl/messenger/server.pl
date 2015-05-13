#!/usr/bin/env perl
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

use strict;
use warnings;
use Getopt::Long;
use Pod::Usage;

use qpid_proton;

my $help = 0;
my $man = 0;

GetOptions(
    man => \$man,
    "help|?" => \$help
    ) or pod2usage(2);

pod2usage(1) if $help;
pod2usage(-exitval => 0, -verbose => 2) if $man;

pod2usage(2) unless scalar(@ARGV);

# create a messenger for receiving and holding
# incoming messages
our $messenger = new qpid::proton::Messenger;
$messenger->start;

# subscribe the messenger to all addresses specified sources
foreach (@ARGV) {
    $messenger->subscribe($_);
}

sub dispatch {
    my $request = $_[0];
    my $reply   = $_[1];

    if ($request->get_subject) {
        $reply->set_subject("Re: " . $request->get_subject);
    }

    $reply->set_properties($request->get_properties);
    print "Dispatched " . $request->get_subject . "\n";
    my $properties = $request->get_properties;
    foreach (keys %{$properties}) {
        my $value = $properties->{%_};
        print "\t$_: $value\n";
    }
}

our $message = new qpid::proton::Message;
our $reply   = new qpid::proton::Message;

while(1) {
    $messenger->receive(1) if $messenger->incoming < 10;

    if ($messenger->incoming > 0) {
        $messenger->get($message);

        if ($message->get_reply_to) {
            print $message->get_reply_to . "\n";
            $reply->set_address($message->get_reply_to);
            $reply->set_correlation_id($message->get_correlation_id);
            $reply->set_body($message->get_body);
        }
        dispatch($message, $reply);
        $messenger->put($reply);
        $messenger->send;
    }
}

$message->stop;

__END__

=head1 NAME

server - Proton example server application for Perl.

=head1 SYNOPSIS

server.pl [OPTIONS] <addr1> ... <addrn>

 Options:
   --help - This help message.
   --man  - Show the full documentation.

=over 8

=item B<--help>

Prints a brief help message and exits.

=item B<--man>

Prints the man page and exits.

=back

=head2 ADDRESS

The form an address takes is:

[amqp://]<domain>[/name]

=cut
