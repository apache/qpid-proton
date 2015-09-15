Simple performance tests.

quick_perf coordinates two processes: a simple fast echo "server" and
a client that sends and receives simple binary messages over a single
connection on the loopback interface.  The latter is timed.  This
provides a crude view of the overhead of the Proton library alone
(CMake target "quick_perf_c") or with a language binding.  It is most
useful for verifying a lack of performance degradation on a large
ckeckin or between releases.  It probably says little about expected
performance on a physical network or for a particular application.
