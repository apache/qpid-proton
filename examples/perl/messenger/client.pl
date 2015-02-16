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

my $reply_to = "~/replies";
my $help = 0;
my $man = 0;

GetOptions(
    "reply_to=s", \$reply_to,
    man => \$man,
    "help|?" => \$help
    ) or pod2usage(2);
pod2usage(1) if $help;
pod2usage(-exitval => 0, -verbose => 2) if $man;

# get the address to use and show help if it's missing
my $address = $ARGV[0];
pod2usage(1) if !$address;

my $messenger = new qpid::proton::Messenger();
$messenger->start;

my $message = new qpid::proton::Message();
$message->set_address($address);
$message->set_reply_to($reply_to);
$message->set_subject("Subject");
$message->set_content("Yo!");

print "Sending to: $address\n";

$messenger->put($message);
$messenger->send;

if($reply_to =~ /^~\//) {
    print "Waiting on returned message.\n";
    $messenger->receive(1);

    $messenger->get($message);
    print $message->get_address . " " . $message->get_subject . "\n";
}

$messenger->stop;

__END__

=head1 NAME

client - Proton example application for Perl.

=head1 SYNOPSIS

client.pl [OPTIONS] <address> <subject>

 Options:
   --reply_to - The reply to address to be used. (default: ~/replies)
   --help     - This help message.
   --man      - Show the full docementation.

=over 8

=item B<--reply_to>

Specifies the reply address to be used for responses from the server.

=item B<--help>

Prints a brief help message and exits.

=item B<--man>

Prints the man page and exits.

=back

=head2 ADDRESS

The form an address takes is:

[amqp://]<domain>[/name]

=cut
