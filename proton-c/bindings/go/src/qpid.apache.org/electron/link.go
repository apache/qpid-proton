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

// Settings associated with a link
type LinkSettings interface {
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
}

// LinkOption can be passed when creating a sender or receiver link to set optional configuration.
type LinkOption func(*linkSettings)

// Source returns a LinkOption that sets address that messages are coming from.
func Source(s string) LinkOption { return func(l *linkSettings) { l.source = s } }

// Target returns a LinkOption that sets address that messages are going to.
func Target(s string) LinkOption { return func(l *linkSettings) { l.target = s } }

// LinkName returns a LinkOption that sets the link name.
func LinkName(s string) LinkOption { return func(l *linkSettings) { l.target = s } }

// SndSettle returns a LinkOption that sets the send settle mode
func SndSettle(m SndSettleMode) LinkOption { return func(l *linkSettings) { l.sndSettle = m } }

// RcvSettle returns a LinkOption that sets the send settle mode
func RcvSettle(m RcvSettleMode) LinkOption { return func(l *linkSettings) { l.rcvSettle = m } }

// SndSettleMode returns a LinkOption that defines when the sending end of the
// link settles message delivery.
type SndSettleMode proton.SndSettleMode

// Capacity returns a LinkOption that sets the link capacity
func Capacity(n int) LinkOption { return func(l *linkSettings) { l.capacity = n } }

// Prefetch returns a LinkOption that sets a receivers pre-fetch flag. Not relevant for a sender.
func Prefetch(p bool) LinkOption { return func(l *linkSettings) { l.prefetch = p } }

// AtMostOnce returns a LinkOption that sets "fire and forget" mode, messages
// are sent but no acknowledgment is received, messages can be lost if there is
// a network failure. Sets SndSettleMode=SendSettled and RcvSettleMode=RcvFirst
func AtMostOnce() LinkOption {
	return func(l *linkSettings) {
		SndSettle(SndSettled)(l)
		RcvSettle(RcvFirst)(l)
	}
}

// AtLeastOnce returns a LinkOption that requests acknowledgment for every
// message, acknowledgment indicates the message was definitely received. In the
// event of a failure, unacknowledged messages can be re-sent but there is a
// chance that the message will be received twice in this case.  Sets
// SndSettleMode=SndUnsettled and RcvSettleMode=RcvFirst
func AtLeastOnce() LinkOption {
	return func(l *linkSettings) {
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

type linkSettings struct {
	source    string
	target    string
	linkName  string
	isSender  bool
	sndSettle SndSettleMode
	rcvSettle RcvSettleMode
	capacity  int
	prefetch  bool
	session   *session
	pLink     proton.Link
}

type link struct {
	endpoint
	linkSettings
}

func (l *linkSettings) Source() string           { return l.source }
func (l *linkSettings) Target() string           { return l.target }
func (l *linkSettings) LinkName() string         { return l.linkName }
func (l *linkSettings) IsSender() bool           { return l.isSender }
func (l *linkSettings) IsReceiver() bool         { return !l.isSender }
func (l *linkSettings) SndSettle() SndSettleMode { return l.sndSettle }
func (l *linkSettings) RcvSettle() RcvSettleMode { return l.rcvSettle }

func (l *link) Session() Session       { return l.session }
func (l *link) Connection() Connection { return l.session.Connection() }
func (l *link) engine() *proton.Engine { return l.session.connection.engine }
func (l *link) handler() *handler      { return l.session.connection.handler }

// Open a link and return the linkSettings.
func makeLocalLink(sn *session, isSender bool, setting ...LinkOption) (linkSettings, error) {
	l := linkSettings{
		isSender: isSender,
		capacity: 1,
		prefetch: false,
		session:  sn,
	}
	for _, set := range setting {
		set(&l)
	}
	if l.linkName == "" {
		l.linkName = l.session.connection.container.nextLinkName()
	}
	if l.IsSender() {
		l.pLink = l.session.pSession.Sender(l.linkName)
	} else {
		l.pLink = l.session.pSession.Receiver(l.linkName)
	}
	if l.pLink.IsNil() {
		return l, fmt.Errorf("cannot create link %s", l.pLink)
	}
	l.pLink.Source().SetAddress(l.source)
	l.pLink.Target().SetAddress(l.target)
	l.pLink.SetSndSettleMode(proton.SndSettleMode(l.sndSettle))
	l.pLink.SetRcvSettleMode(proton.RcvSettleMode(l.rcvSettle))
	l.pLink.Open()
	return l, nil
}

type incomingLink struct {
	incoming
	linkSettings
	pLink proton.Link
	sn    *session
}

// Set up a link from an incoming proton.Link.
func makeIncomingLink(sn *session, pLink proton.Link) incomingLink {
	l := incomingLink{
		incoming: makeIncoming(pLink),
		linkSettings: linkSettings{
			isSender:  pLink.IsSender(),
			source:    pLink.RemoteSource().Address(),
			target:    pLink.RemoteTarget().Address(),
			linkName:  pLink.Name(),
			sndSettle: SndSettleMode(pLink.RemoteSndSettleMode()),
			rcvSettle: RcvSettleMode(pLink.RemoteRcvSettleMode()),
			capacity:  1,
			prefetch:  false,
			pLink:     pLink,
			session:   sn,
		},
	}
	return l
}

// Not part of Link interface but use by Sender and Receiver.
func (l *link) Credit() (credit int, err error) {
	err = l.engine().InjectWait(func() error {
		if l.Error() != nil {
			return l.Error()
		}
		credit = l.pLink.Credit()
		return nil
	})
	return
}

// Not part of Link interface but use by Sender and Receiver.
func (l *link) Capacity() int { return l.capacity }

func (l *link) Close(err error) {
	l.engine().Inject(func() {
		if l.Error() == nil {
			localClose(l.pLink, err)
		}
	})
}
