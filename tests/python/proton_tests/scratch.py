  def xxx_test_reopen_on_same_session(self):
    ssn1 = self.snd.session
    ssn2 = self.rcv.session

    self.snd.open()
    self.rcv.open()
    self.pump()

    assert self.snd.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert self.rcv.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

    self.snd.close()
    self.rcv.close()
    self.pump()

    assert self.snd.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED
    assert self.rcv.state == Endpoint.LOCAL_CLOSED | Endpoint.REMOTE_CLOSED

    print self.snd._link
    self.snd = ssn1.sender("test-link")
    print self.snd._link
    self.rcv = ssn2.receiver("test-link")
    self.snd.open()
    self.rcv.open()
    self.pump()

    assert self.snd.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE
    assert self.rcv.state == Endpoint.LOCAL_ACTIVE | Endpoint.REMOTE_ACTIVE

class SessionPipelineTest(PeerTest):

  def xxx_test(self):
    self.connection.open()
    self.peer.open()
    self.pump()
    ssn = self.connection.session()
    ssn.open()
    self.pump()
    peer_ssn = self.peer.session_head(0)
    ssn.close()
    self.pump()
    peer_ssn.close()
    self.peer.close()
    self.pump()
