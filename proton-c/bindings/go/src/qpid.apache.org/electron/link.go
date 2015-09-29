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
	"qpid.apache.org/internal"
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

// LinkSetting is a function that sets a link property. Passed when creating
// a Sender or Receiver, do not use at any other time.
type LinkSetting func(Link)

// Source sets address that messages are coming from.
func Source(s string) LinkSetting { return func(l Link) { l.(*link).source = s } }

// Target sets address that messages are going to.
func Target(s string) LinkSetting { return func(l Link) { l.(*link).target = s } }

// LinkName sets the link name.
func LinkName(s string) LinkSetting { return func(l Link) { l.(*link).target = s } }

// SndSettle sets the send settle mode
func SndSettle(m SndSettleMode) LinkSetting { return func(l Link) { l.(*link).sndSettle = m } }

// RcvSettle sets the send settle mode
func RcvSettle(m RcvSettleMode) LinkSetting { return func(l Link) { l.(*link).rcvSettle = m } }

// SndSettleMode defines when the sending end of the link settles message delivery.
type SndSettleMode proton.SndSettleMode

// Capacity sets the link capacity
func Capacity(n int) LinkSetting { return func(l Link) { l.(*link).capacity = n } }

// Prefetch sets a receivers pre-fetch flag. Not relevant for a sender.
func Prefetch(p bool) LinkSetting { return func(l Link) { l.(*link).prefetch = p } }

// AtMostOnce sets "fire and forget" mode, messages are sent but no
// acknowledgment is received, messages can be lost if there is a network
// failure. Sets SndSettleMode=SendSettled and RcvSettleMode=RcvFirst
func AtMostOnce() LinkSetting {
	return func(l Link) {
		SndSettle(SndSettled)(l)
		RcvSettle(RcvFirst)(l)
	}
}

// AtLeastOnce requests acknowledgment for every message, acknowledgment
// indicates the message was definitely received. In the event of a
// failure, unacknowledged messages can be re-sent but there is a chance
// that the message will be received twice in this case.
// Sets SndSettleMode=SndUnsettled and RcvSettleMode=RcvFirst
func AtLeastOnce() LinkSetting {
	return func(l Link) {
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

	inAccept bool
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
func localLink(sn *session, isSender bool, setting ...LinkSetting) (*link, error) {
	l := &link{
		session:  sn,
		isSender: isSender,
		capacity: 1,
		prefetch: false,
		done:     make(chan struct{}),
	}
	for _, set := range setting {
		set(l)
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
		l.err.Set(internal.Errorf("cannot create link %s", l))
		return nil, l.err.Get()
	}
	l.eLink.Source().SetAddress(l.source)
	l.eLink.Target().SetAddress(l.target)
	l.eLink.SetSndSettleMode(proton.SndSettleMode(l.sndSettle))
	l.eLink.SetRcvSettleMode(proton.RcvSettleMode(l.rcvSettle))
	l.str = l.eLink.String()
	l.eLink.Open()
	return l, nil
}

// Set up a link from an incoming proton.Link.
func incomingLink(sn *session, eLink proton.Link) link {
	l := link{
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
