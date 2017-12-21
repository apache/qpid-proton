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
    return cproton_perl::pn_messenger_name($self->{_impl});
}

sub set_timeout {
    my ($self) = @_;
    my $timeout = $_[1];

    $timeout = 0 if !defined($timeout);
    $timeout = int($timeout);

    cproton_perl::pn_messenger_set_timeout($self->{_impl}, $timeout);
}

sub get_timeout {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_timeout($self->{_impl});
}

sub set_outgoing_window {
    my ($self) = @_;
    my $window = $_[1];

    $window = 0 if !defined($window);
    $window = int($window);

    qpid::proton::check_for_error(cproton_perl::pn_messenger_set_outgoing_window($self->{_impl}, $window), $self);
}

sub get_outgoing_window {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_outgoing_window($self->{_impl});
}

sub set_incoming_window{
    my ($self) = @_;
    my $window = $_[1];

    $window = 0 if !defined($window);
    $window = int($window);

    qpid::proton::check_for_error(cproton_perl::pn_messenger_set_incoming_window($self->{_impl}, $window), $self);
}

sub get_incoming_window {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_incoming_window($self->{_impl});
}

sub get_error {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $text = cproton_perl::pn_error_text(cproton_perl::pn_messenger_error($impl));

    return $text || "";
}

sub get_errno {
    my ($self) = @_;
    return cproton_perl::pn_messenger_errno($self->{_impl});
}

sub start {
    my ($self) = @_;
    qpid::proton::check_for_error(cproton_perl::pn_messenger_start($self->{_impl}), $self);
}

sub stop {
    my ($self) = @_;
    qpid::proton::check_for_error(cproton_perl::pn_messenger_stop($self->{_impl}), $self);
}

sub stopped {
    my ($self) = @_;
    my $impl = $self->{_impl};

    return cproton_perl::pn_messenger_stopped($impl);
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

sub set_password {
    my ($self) = @_;

    qpid::proton::check_for_error(cproton_perl::pn_messenger_set_password($self->{_impl}, $_[1]), $self);
}

sub get_password {
    my ($self) = @_;
    return cproton_perl::pn_messenger_get_password($self->{_impl});
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
    my $impl = $self->{_impl};
    my $message = $_[1];

    $message->preencode();
    my $msgimpl = $message->get_impl();
    qpid::proton::check_for_error(cproton_perl::pn_messenger_put($impl, $msgimpl), $self);

    my $tracker = $self->get_outgoing_tracker();
    return $tracker;
}

sub get_outgoing_tracker {
    my ($self) = @_;
    my $impl = $self->{_impl};

    my $tracker = cproton_perl::pn_messenger_outgoing_tracker($impl);
    if ($tracker != -1) {
        return qpid::proton::Tracker->new($tracker);
    } else {
        return undef;
    }
}

sub send {
    my ($self) = @_;
    my $n = $_[1];
    $n = -1 if !defined $n;
    qpid::proton::check_for_error(cproton_perl::pn_messenger_send($self->{_impl}, $n), $self);
}

sub get {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $message = $_[1] || new proton::Message();

    qpid::proton::check_for_error(cproton_perl::pn_messenger_get($impl, $message->get_impl()), $self);
    $message->postdecode();

    my $tracker = $self->get_incoming_tracker();
    return $tracker;
}

sub get_incoming_tracker {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $result = undef;

    my $tracker = cproton_perl::pn_messenger_incoming_tracker($impl);
    if ($tracker != -1) {
        $result = new qpid::proton::Tracker($tracker);
    }

    return $result;
}

sub receive {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $n = $_[1] || -1;

    qpid::proton::check_for_error(cproton_perl::pn_messenger_recv($impl, $n), $self);
}

sub set_blocking {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $blocking = int($_[1] || 0);

    qpid::proton::check_for_error(cproton_perl::pn_messenger_set_blocking($impl, $blocking));
}

sub get_blocking {
    my ($self) = @_;
    my $impl = $self->{_impl};

    return cproton_perl::pn_messenger_is_blocking($impl);
}

sub work {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $timeout = $_[1];

    if (!defined($timeout)) {
        $timeout = -1;
    } else {
        $timeout = int($timeout * 1000);
    }
    my $err = cproton_perl::pn_messenger_work($impl, $timeout);
    if ($err == qpid::proton::Errors::TIMEOUT) {
        return 0;
    } else {
        qpid::proton::check_for_error($err);
        return 1;
    }
}

sub interrupt {
    my ($self) = @_;

    qpid::proton::check_for_error(cproton_perl::pn_messenger_interrupt($self->{_impl}), $self);
}

sub outgoing {
    my ($self) = @_;
    return cproton_perl::pn_messenger_outgoing($self->{_impl});
}

sub incoming {
    my ($self) = @_;
    return cproton_perl::pn_messenger_incoming($self->{_impl});
}

sub route {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $pattern = $_[1];
    my $address = $_[2];

    qpid::proton::check_for_error(cproton_perl::pn_messenger_route($impl, $pattern, $address));
}

sub rewrite {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $pattern = $_[1];
    my $address = $_[2];

    qpid::proton::check_for_error(cproton_perl::pn_messenger_rewrite($impl, $pattern, $address));
}

sub accept {
    my ($self) = @_;
    my $tracker = $_[1];
    my $flags = 0;
    if (!defined $tracker) {
        $tracker = cproton_perl::pn_messenger_incoming_tracker($self->{_impl});
        $flags = $cproton_perl::PN_CUMULATIVE;
    } else {
        $tracker = $tracker->get_impl;
    }

    qpid::proton::check_for_error(cproton_perl::pn_messenger_accept($self->{_impl}, $tracker, $flags), $self);
}

sub reject {
    my ($self) = @_;
    my $tracker = $_[1];
    my $flags = 0;
    if (!defined $tracker) {
        $tracker = cproton_perl::pn_messenger_incoming_tracker($self->{_impl});
        $flags = $cproton_perl::PN_CUMULATIVE;
    }
    qpid::proton::check_for_error(cproton_perl::pn_messenger_reject($self->{_impl}, $tracker, $flags), $self);
}

sub status {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $tracker = $_[1];

    if (!defined($tracker)) {
        $tracker = $self->get_incoming_tracker();
    }

    return cproton_perl::pn_messenger_status($impl, $tracker->get_impl);
}

sub settle {
    my ($self) = @_;
    my $impl = $self->{_impl};
    my $tracker = $_[1];
    my $flag = 0;

    if (!defined($tracker)) {
        $tracker = $self->get_incoming_tracker();
        $flag = $cproton_perl::PN_CUMULATIVE;
    }

    cproton_perl::pn_messenger_settle($impl, $tracker->get_impl, $flag);
}

1;
