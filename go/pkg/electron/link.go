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
	"github.com/apache/qpid-proton/go/pkg/amqp"
	"github.com/apache/qpid-proton/go/pkg/proton"
	"time"
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

	// Filter for the link
	Filter() map[amqp.Symbol]interface{}

	// Advanced settings for the source
	SourceSettings() TerminusSettings

	// Advanced settings for the target
	TargetSettings() TerminusSettings
}

// LinkOption can be passed when creating a sender or receiver link to set optional configuration.
type LinkOption func(*linkSettings)

// Source returns a LinkOption that sets address that messages are coming from.
func Source(s string) LinkOption { return func(l *linkSettings) { l.source = s } }

// Target returns a LinkOption that sets address that messages are going to.
func Target(s string) LinkOption { return func(l *linkSettings) { l.target = s } }

// LinkName returns a LinkOption that sets the link name.
func LinkName(s string) LinkOption { return func(l *linkSettings) { l.linkName = s } }

// SndSettle returns a LinkOption that sets the send settle mode
func SndSettle(m SndSettleMode) LinkOption { return func(l *linkSettings) { l.sndSettle = m } }

// RcvSettle returns a LinkOption that sets the send settle mode
func RcvSettle(m RcvSettleMode) LinkOption { return func(l *linkSettings) { l.rcvSettle = m } }

// Capacity returns a LinkOption that sets the link capacity
func Capacity(n int) LinkOption { return func(l *linkSettings) { l.capacity = n } }

// Prefetch returns a LinkOption that sets a receivers pre-fetch flag. Not relevant for a sender.
func Prefetch(p bool) LinkOption { return func(l *linkSettings) { l.prefetch = p } }

// DurableSubscription returns a LinkOption that configures a Receiver as a named durable
// subscription.  The name overrides (and is overridden by) LinkName() so you should normally
// only use one of these options.
func DurableSubscription(name string) LinkOption {
	return func(l *linkSettings) {
		l.linkName = name
		l.sourceSettings.Durability = proton.Deliveries
		l.sourceSettings.Expiry = proton.ExpireNever
	}
}

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

// Filter returns a LinkOption that sets a filter.
func Filter(m map[amqp.Symbol]interface{}) LinkOption {
	return func(l *linkSettings) { l.filter = m }
}

// SourceSettings returns a LinkOption that sets all the SourceSettings.
// Note: it will override the source address set by a Source() option
func SourceSettings(ts TerminusSettings) LinkOption {
	return func(l *linkSettings) { l.sourceSettings = ts }
}

// TargetSettings returns a LinkOption that sets all the TargetSettings.
// Note: it will override the target address set by a Target() option
func TargetSettings(ts TerminusSettings) LinkOption {
	return func(l *linkSettings) { l.targetSettings = ts }
}

// SndSettleMode defines when the sending end of the link settles message delivery.
type SndSettleMode proton.SndSettleMode

const (
	// Messages are sent unsettled
	SndUnsettled = SndSettleMode(proton.SndUnsettled)
	// Messages are sent already settled
	SndSettled = SndSettleMode(proton.SndSettled)
	// Sender can send either unsettled or settled messages.
	SndMixed = SndSettleMode(proton.SndMixed)
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
	source         string
	sourceSettings TerminusSettings
	target         string
	targetSettings TerminusSettings
	linkName       string
	isSender       bool
	sndSettle      SndSettleMode
	rcvSettle      RcvSettleMode
	capacity       int
	prefetch       bool
	filter         map[amqp.Symbol]interface{}
	session        *session
	pLink          proton.Link
}

// Advanced AMQP settings for the source or target of a link.
// Usually these can be set via a more descriptive LinkOption, e.g. DurableSubscription()
// and do not need to be set/examined directly.
type TerminusSettings struct {
	Durability proton.Durability
	Expiry     proton.ExpiryPolicy
	Timeout    time.Duration
	Dynamic    bool
}

func makeTerminusSettings(t proton.Terminus) TerminusSettings {
	return TerminusSettings{
		Durability: t.Durability(),
		Expiry:     t.ExpiryPolicy(),
		Timeout:    t.Timeout(),
		Dynamic:    t.IsDynamic(),
	}
}

type link struct {
	endpoint
	linkSettings
}

func (l *linkSettings) Source() string                      { return l.source }
func (l *linkSettings) Target() string                      { return l.target }
func (l *linkSettings) LinkName() string                    { return l.linkName }
func (l *linkSettings) IsSender() bool                      { return l.isSender }
func (l *linkSettings) IsReceiver() bool                    { return !l.isSender }
func (l *linkSettings) SndSettle() SndSettleMode            { return l.sndSettle }
func (l *linkSettings) RcvSettle() RcvSettleMode            { return l.rcvSettle }
func (l *linkSettings) Filter() map[amqp.Symbol]interface{} { return l.filter }
func (l *linkSettings) SourceSettings() TerminusSettings    { return l.sourceSettings }
func (l *linkSettings) TargetSettings() TerminusSettings    { return l.targetSettings }

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

	if len(l.filter) > 0 {
		if err := l.pLink.Source().Filter().Marshal(l.filter); err != nil {
			panic(err) // Shouldn't happen
		}
	}
	l.pLink.Source().SetDurability(l.sourceSettings.Durability)
	l.pLink.Source().SetExpiryPolicy(l.sourceSettings.Expiry)
	l.pLink.Source().SetTimeout(l.sourceSettings.Timeout)
	l.pLink.Source().SetDynamic(l.sourceSettings.Dynamic)

	l.pLink.Target().SetAddress(l.target)
	l.pLink.Target().SetDurability(l.targetSettings.Durability)
	l.pLink.Target().SetExpiryPolicy(l.targetSettings.Expiry)
	l.pLink.Target().SetTimeout(l.targetSettings.Timeout)
	l.pLink.Target().SetDynamic(l.targetSettings.Dynamic)

	l.pLink.SetSndSettleMode(proton.SndSettleMode(l.sndSettle))
	l.pLink.SetRcvSettleMode(proton.RcvSettleMode(l.rcvSettle))
	l.pLink.Open()
	return l, nil
}

func makeIncomingLinkSettings(pLink proton.Link, sn *session) linkSettings {
	l := linkSettings{
		isSender:       pLink.IsSender(),
		source:         pLink.RemoteSource().Address(),
		sourceSettings: makeTerminusSettings(pLink.RemoteSource()),
		target:         pLink.RemoteTarget().Address(),
		targetSettings: makeTerminusSettings(pLink.RemoteTarget()),
		linkName:       pLink.Name(),
		sndSettle:      SndSettleMode(pLink.RemoteSndSettleMode()),
		rcvSettle:      RcvSettleMode(pLink.RemoteRcvSettleMode()),
		capacity:       1,
		prefetch:       false,
		pLink:          pLink,
		session:        sn,
	}
	filter := l.pLink.RemoteSource().Filter()
	if !filter.Empty() {
		filter.Unmarshal(&l.filter) // TODO aconway 2017-06-08: ignoring errors
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
	_ = l.engine().Inject(func() {
		if l.Error() == nil {
			localClose(l.pLink, err)
		}
	})
}
