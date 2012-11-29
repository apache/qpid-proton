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
use cproton_perl;

package qpid::proton::Messenger;

sub new {
    my ($class) = @_;
    my ($self) = {};

    my $impl = cproton_perl::pn_messenger($_[1]);
    $self->{_impl} = $impl;

    bless $self, $class;
    return $self;
}

sub DESTROY {
    my ($self) = @_;
    cproton_perl::pn_messenger_stop($self->{_impl});
    cproton_perl::pn_messenger_free($self->{_impl});
}

sub get_name {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_name($self->{_impl});
}

sub set_timeout {
    my ($self) = @_;
    cproton_perl::pn_messenger_set_timeout($self->{_impl}, $_[1]);
}

sub get_timeout {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_timeout($self->{_impl});
}

sub get_error {
    my ($self) = @_;
    return cproton_perl::pn_messenger_error($self->{_impl});
}

sub get_errno {
    my ($self) = @_;
    return cproton_perl::pn_messenger_errno($self->{_impl});
}

sub start {
    my ($self) = @_;
    cproton_perl::pn_messenger_start($self->{_impl});
}

sub stop {
    my ($self) = @_;
    cproton_perl::pn_messenger_stop($self->{_impl});
}

sub subscribe {
    my ($self) = @_;
    cproton_perl::pn_messenger_subscribe($self->{_impl}, $_[1]);
}

sub set_certificate {
    my ($self) = @_;
    cproton_perl::pn_messenger_set_certificate($self->{_impl}, $_[1]);
}

sub get_certificate {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_certificate($self->{_impl});
}

sub set_private_key {
    my ($self) = @_;
    cproton_perl::pn_messenger_set_private_key($self->{_impl}, $_[1]);
}

sub get_private_key {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_private_key($self->{_impl});
}

sub set_trusted_certificates {
    my ($self) = @_;
    cproton_perl::pn_messenger_set_trusted_certificates($self->{_impl}, $_[1]);
}

sub get_trusted_certificates {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_trusted_certificates($self->{_impl});
}

sub put {
    my ($self) = @_;
    my $message = $_[1];
    cproton_perl::pn_messenger_put($self->{_impl}, $message->get_impl);
}

sub send {
    my ($self) = @_;
    cproton_perl::pn_messenger_send($self->{_impl});
}

sub get {
    my ($self) = @_;

    my $message = $_[1] || new proton::Message();
    cproton_perl::pn_messenger_get($self->{_impl}, $message->get_impl());
    return $message;
}

sub receive {
    my ($self) = @_;
    cproton_perl::pn_messenger_recv($self->{_impl}, $_[1]);
}

sub outgoing {
    my ($self) = @_;
    return cproton_perl::pn_messenger_outgoing($self->{_impl});
}

sub incoming {
    my ($self) = @_;
    return cproton_perl::pn_messenger_incoming($self->{_impl});
}

1;
