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

package concurrent

import (
	"qpid.apache.org/proton"
	"qpid.apache.org/proton/internal"
)

type LinkSettings struct {
	// Source address that messages come from.
	Source string
	// Target address that messages are going to.
	Target string

	// Unique (per container) name of the link.
	// Leave blank to have the container generate a unique name automatically.
	Name string

	// SndSettleMode defines when the sending end of the link settles message delivery.
	// Can set via AtMostOnce or AtLeastOnce.
	SndSettleMode SndSettleMode

	// RcvSettleMode defines when the receiving end of the link settles message delivery.
	RcvSettleMode RcvSettleMode
}

// AtMostOnce sets "fire and forget" mode, messages are sent but no
// acknowledgment is received, messages can be lost if there is a network
// failure. Sets SndSettleMode=SendSettled and RcvSettleMode=RcvFirst
func (s *LinkSettings) AtMostOnce() {
	s.SndSettleMode = SndSettled
	s.RcvSettleMode = RcvFirst
}

// AtLeastOnce requests acknowledgment for every message, acknowledgment
// indicates the message was definitely received. In the event of a
// failure, unacknowledged messages can be re-sent but there is a chance
// that the message will be received twice in this case.
// Sets SndSettleMode=SndUnsettled and RcvSettleMode=RcvFirst
func (s *LinkSettings) AtLeastOnce() {
	s.SndSettleMode = SndUnsettled
	s.RcvSettleMode = RcvFirst
}

// Link is the common interface for Sender and Receiver links.
type Link interface {
	Endpoint

	// Settings for this link.
	Settings() LinkSettings

	IsSender() bool
	IsReceiver() bool

	IsOpen() bool

	// Credit indicates how many messages the receiving end of the link can accept.
	//
	// A Receiver adds credit automatically when it can accept more messages.
	//
	// On a Sender credit can be negative, meaning that messages in excess of the
	// receiver's credit limit have been buffered locally till credit is available.
	Credit() (int, error)

	// Called in event loop on closed event.
	closed(err error)
}

// SndSettleMode defines when the sending end of the link settles message delivery.
// Can set via AtMostOnce or AtLeastOnce.
type SndSettleMode proton.SndSettleMode

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

// Implement Link interface, for embedding in sender and receiver.
//
// Link creation: there are two ways to create a link.
//
// Session.NewSender() and Session.NewReceiver() create a "local" link which has
// the session and isSender fields set. The user can set properties like Name,
// Target and Source. On Open() the eLink is created and the properties are set
// on the eLink.
//
// An "incoming" is created by the connection. with session, isSender, name,
// source, target all set from the incoming eLink, these properties cannot be
// changed by the user. There may be other properties (e.g. Receiver.SetCapacity())
// that can be set by the user before Open().
//
type link struct {
	endpoint

	settings LinkSettings
	session  *session
	eLink    proton.Link
	isOpen   bool
	isSender bool
}

func (l *link) Settings() LinkSettings { return l.settings }
func (l *link) IsSender() bool         { return l.isSender }
func (l *link) IsReceiver() bool       { return !l.isSender }
func (l *link) IsOpen() bool           { return l.isOpen }
func (l *link) Session() Session       { return l.session }
func (l *link) Connection() Connection { return l.session.Connection() }

func (l *link) engine() *proton.Engine { return l.session.connection.engine }
func (l *link) handler() *handler      { return l.session.connection.handler }

// initLocal initializes a local link associated with a session.
// Call in proton goroutine
func makeLocalLink(sn *session, isSender bool, settings LinkSettings) (link, error) {
	var l link
	l.session = sn
	l.settings = settings
	l.isSender = isSender
	if l.settings.Name == "" {
		l.settings.Name = l.session.connection.container.nextLinkName()
	}
	if l.IsSender() {
		l.eLink = l.session.eSession.Sender(l.settings.Name)
	} else {
		l.eLink = l.session.eSession.Receiver(l.settings.Name)
	}
	if l.eLink.IsNil() {
		return l, l.setError(internal.Errorf("cannot create link %s", l))
	}
	l.setSettings()
	return l, nil
}

// Set local end of the link to match LinkSettings.
func (l *link) setSettings() {
	l.eLink.Source().SetAddress(l.settings.Source)
	l.eLink.Target().SetAddress(l.settings.Target)
	l.eLink.SetSndSettleMode(proton.SndSettleMode(l.settings.SndSettleMode))
	l.eLink.SetRcvSettleMode(proton.RcvSettleMode(l.settings.RcvSettleMode))
	l.str = l.eLink.String()
}

// initIncoming sets link settings from an incoming proton.Link.
// Call in proton goroutine
func makeIncomingLink(sn *session, eLink proton.Link) link {
	l := link{
		session:  sn,
		isSender: eLink.IsSender(),
		eLink:    eLink,
		settings: LinkSettings{
			Source:        eLink.RemoteSource().Address(),
			Target:        eLink.RemoteTarget().Address(),
			Name:          eLink.Name(),
			SndSettleMode: SndSettleMode(eLink.RemoteSndSettleMode()),
			RcvSettleMode: RcvSettleMode(eLink.RemoteRcvSettleMode()),
		},
	}
	l.setSettings()
	return l
}

func (l *link) setPanicIfOpen() {
	if l.IsOpen() {
		panic(internal.Errorf("link is already open %s", l))
	}
}

// open a link, local or incoming. Call in proton goroutine
func (l *link) open() error {
	if l.Error() != nil {
		return l.Error()
	}
	l.eLink.Open()
	l.isOpen = true
	return nil
}

// Called in proton goroutine
func (l *link) closed(err error) {
	l.setError(err)
	if l.eLink.State().RemoteActive() {
		if l.Error() != nil {
			l.eLink.Condition().SetError(l.Error())
		}
		l.eLink.Close()
	}
	l.setError(Closed) // If no error set, mark as closed.
}

func (l *link) Credit() (credit int, err error) {
	err = l.engine().InjectWait(func() error {
		credit = l.eLink.Credit()
		return nil
	})
	return
}

func (l *link) Close(err error) {
	l.engine().Inject(func() { localClose(l.eLink, err) })
}
