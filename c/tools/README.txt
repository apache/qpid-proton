This directory contains applications built using proton.  These
applications are used by the testbed for soak tests.  See the
"soak.py" file for the tests that utilize these applications.

These applications can be used standalone to generate or consume
message traffic.

Contents:

msgr-send - this Messenger-based application generates message
   traffic, and can be configured to consume responses.

msgr-recv - this Messenger-based application consumes message traffic,
   and can be configured to forward or reply to received messages.
