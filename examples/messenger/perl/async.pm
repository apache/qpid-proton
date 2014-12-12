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

use qpid_proton;

package async::CallbackAdapter;

sub new {
    my ($class) = @_;
    my ($self) = {};

    my $messenger = $_[1];

    $self->{_messenger} = $messenger;
    $messenger->set_blocking(0);
    $messenger->set_incoming_window(1024);
    $messenger->set_outgoing_window(1024);

    my $message = qpid::proton::Message->new();
    $self->{_message} = $message;
    $self->{_incoming} = $message;
    $self->{_tracked} = {};

    bless $self, $class;
    return $self;
}

sub run {
    my ($self) = @_;

    $self->{_running} = 1;

    my $messenger = $self->{_messenger};

    $messenger->start();
    $self->on_start();

    do {
        $messenger->work;
        $self->process_outgoing;
        $self->process_incoming;
    } while($self->{_running});

    $messenger->stop();

    while(!$messenger->stopped()) {
        $messenger->work;
        $self->process_outgoing;
        $self->process_incoming;
    }

    $self->on_stop();
}

sub stop {
    my ($self) = @_;

    $self->{_running} = 0;
}

sub process_outgoing {
    my ($self) = @_;
    my $tracked = $self->{_tracked};

    foreach $key (keys %{$tracked}) {
        my $on_status = $tracked->{$key};
        if (defined($on_status)) {
            if (!($on_status eq qpid::proton::Tracker::PENDING)) {
                $self->$on_status($status);
                $self->{_messenger}->settle($t);
                # delete the settled item
                undef $tracked->{$key};
            }
        }
    }
}

sub process_incoming {
    my ($self) = @_;
    my $messenger = $self->{_messenger};

    while ($messenger->incoming > 0) {
        my $message = $self->{_message};
        my $t = $messenger->get($message);

        $self->on_receive($message);
        $messenger->accept($t);
    }
}

sub send {
    my ($self) = @_;
    my $messenger = $self->{_messenger};
    my $tracked = $self->{_tracked};
    my $message = $_[1];
    my $on_status = $_[2] || undef;

    my $tracker = $messenger->put($message);

    $tracked->{$tracker} = $on_status if (defined($on_status));
}


1;
