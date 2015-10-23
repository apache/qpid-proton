/*
Licensed to the Apache Software Foundation (ASF) under one
or more contributor license agreements.  See the NOTICE file
distributed with this work for additional information
regarding copyright ownership.  The ASF licenses this file
to you under the Apache License, Version 2.0 (the
"License"); you may not use this file except in compliance
with the License.  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing,
software distributed under the License is distributed on an
"AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
KIND, either express or implied.  See the License for the
specific language governing permissions and limitations
under the License.
*/

package electron

import (
	"fmt"
	"qpid.apache.org/proton"
)

// Link is the common interface for AMQP links. Sender and Receiver provide
// more methods for the sending or receiving end of a link respectively.
type Link interface {
	Endpoint

	// Source address that messages are coming from.
	Source() string

	// Target address that messages are going to.
	Target() string

	// Name is a unique name for the link among links between the same
	// containers in the same direction. By default generated automatically.
	LinkName() string

	// IsSender is true if this is the sending end of the link.
	IsSender() bool

	// IsReceiver is true if this is the receiving end of the link.
	IsReceiver() bool

	// SndSettle defines when the sending end of the link settles message delivery.
	SndSettle() SndSettleMode

	// RcvSettle defines when the sending end of the link settles message delivery.
	RcvSettle() RcvSettleMode

	// Session containing the Link
	Session() Session

	// Called in event loop on closed event.
	closed(err error)
	// Called to open a link (local or accepted incoming link)
	open()
}

// LinkOption can be passed when creating a sender or receiver link.
type LinkOption func(*link)

// Source sets address that messages are coming from.
func Source(s string) LinkOption { return func(l *link) { l.source = s } }

// Target sets address that messages are going to.
func Target(s string) LinkOption { return func(l *link) { l.target = s } }

// LinkName sets the link name.
func LinkName(s string) LinkOption { return func(l *link) { l.target = s } }

// SndSettle sets the send settle mode
func SndSettle(m SndSettleMode) LinkOption { return func(l *link) { l.sndSettle = m } }

// RcvSettle sets the send settle mode
func RcvSettle(m RcvSettleMode) LinkOption { return func(l *link) { l.rcvSettle = m } }

// SndSettleMode defines when the sending end of the link settles message delivery.
type SndSettleMode proton.SndSettleMode

// Capacity sets the link capacity
func Capacity(n int) LinkOption { return func(l *link) { l.capacity = n } }

// Prefetch sets a receivers pre-fetch flag. Not relevant for a sender.
func Prefetch(p bool) LinkOption { return func(l *link) { l.prefetch = p } }

// AtMostOnce sets "fire and forget" mode, messages are sent but no
// acknowledgment is received, messages can be lost if there is a network
// failure. Sets SndSettleMode=SendSettled and RcvSettleMode=RcvFirst
func AtMostOnce() LinkOption {
	return func(l *link) {
		SndSettle(SndSettled)(l)
		RcvSettle(RcvFirst)(l)
	}
}

// AtLeastOnce requests acknowledgment for every message, acknowledgment
// indicates the message was definitely received. In the event of a
// failure, unacknowledged messages can be re-sent but there is a chance
// that the message will be received twice in this case.
// Sets SndSettleMode=SndUnsettled and RcvSettleMode=RcvFirst
func AtLeastOnce() LinkOption {
	return func(l *link) {
		SndSettle(SndUnsettled)(l)
		RcvSettle(RcvFirst)(l)
	}
}

const (
	// Messages are sent unsettled
	SndUnsettled = SndSettleMode(proton.SndUnsettled)
	// Messages are sent already settled
	SndSettled = SndSettleMode(proton.SndSettled)
	// Sender can send either unsettled or settled messages.
	SendMixed = SndSettleMode(proton.SndMixed)
)

// RcvSettleMode defines when the receiving end of the link settles message delivery.
type RcvSettleMode proton.RcvSettleMode

const (
	// Receiver settles first.
	RcvFirst = RcvSettleMode(proton.RcvFirst)
	// Receiver waits for sender to settle before settling.
	RcvSecond = RcvSettleMode(proton.RcvSecond)
)

type link struct {
	endpoint

	// Link settings.
	source    string
	target    string
	linkName  string
	isSender  bool
	sndSettle SndSettleMode
	rcvSettle RcvSettleMode
	capacity  int
	prefetch  bool

	session *session
	eLink   proton.Link
	done    chan struct{} // Closed when link is closed
}

func (l *link) Source() string           { return l.source }
func (l *link) Target() string           { return l.target }
func (l *link) LinkName() string         { return l.linkName }
func (l *link) IsSender() bool           { return l.isSender }
func (l *link) IsReceiver() bool         { return !l.isSender }
func (l *link) SndSettle() SndSettleMode { return l.sndSettle }
func (l *link) RcvSettle() RcvSettleMode { return l.rcvSettle }
func (l *link) Session() Session         { return l.session }
func (l *link) Connection() Connection   { return l.session.Connection() }

func (l *link) engine() *proton.Engine { return l.session.connection.engine }
func (l *link) handler() *handler      { return l.session.connection.handler }

// Set up link fields and open the proton.Link
func localLink(sn *session, isSender bool, setting ...LinkOption) (link, error) {
	l := link{
		session:  sn,
		isSender: isSender,
		capacity: 1,
		prefetch: false,
		done:     make(chan struct{}),
	}
	for _, set := range setting {
		set(&l)
	}
	if l.linkName == "" {
		l.linkName = l.session.connection.container.nextLinkName()
	}
	if l.IsSender() {
		l.eLink = l.session.eSession.Sender(l.linkName)
	} else {
		l.eLink = l.session.eSession.Receiver(l.linkName)
	}
	if l.eLink.IsNil() {
		l.err.Set(fmt.Errorf("cannot create link %s", l))
		return l, l.err.Get()
	}
	l.eLink.Source().SetAddress(l.source)
	l.eLink.Target().SetAddress(l.target)
	l.eLink.SetSndSettleMode(proton.SndSettleMode(l.sndSettle))
	l.eLink.SetRcvSettleMode(proton.RcvSettleMode(l.rcvSettle))
	l.str = l.eLink.String()
	l.eLink.Open()
	return l, nil
}

type incomingLink struct {
	incoming
	link
}

// Set up a link from an incoming proton.Link.
func makeIncomingLink(sn *session, eLink proton.Link) incomingLink {
	l := incomingLink{
		link: link{
			session:   sn,
			isSender:  eLink.IsSender(),
			eLink:     eLink,
			source:    eLink.RemoteSource().Address(),
			target:    eLink.RemoteTarget().Address(),
			linkName:  eLink.Name(),
			sndSettle: SndSettleMode(eLink.RemoteSndSettleMode()),
			rcvSettle: RcvSettleMode(eLink.RemoteRcvSettleMode()),
			capacity:  1,
			prefetch:  false,
			done:      make(chan struct{}),
		},
	}
	l.str = eLink.String()
	return l
}

// Called in proton goroutine on closed or disconnected
func (l *link) closed(err error) {
	l.err.Set(err)
	l.err.Set(Closed) // If no error set, mark as closed.
	close(l.done)
}

// Not part of Link interface but use by Sender and Receiver.
func (l *link) Credit() (credit int, err error) {
	err = l.engine().InjectWait(func() error {
		credit = l.eLink.Credit()
		return nil
	})
	return
}

// Not part of Link interface but use by Sender and Receiver.
func (l *link) Capacity() int { return l.capacity }

func (l *link) Close(err error) {
	l.engine().Inject(func() { localClose(l.eLink, err) })
}

func (l *link) open() {
	l.eLink.Open()
}
