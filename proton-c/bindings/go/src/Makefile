# FIXME aconway 2015-04-09: integrate with cmake

#GOFLAGS=-compiler gccgo -gccgoflags "-g -O0"
#GOFLAGS=-gcflags "-N -l"

GENERATED=qpid.apache.org/proton/event/wrappers_gen.go

test: $(GENERATED)
	go test -v $(TESTFLAGS) $(GOFLAGS) qpid.apache.org/proton
	go test -v $(TESTFLAGS) $(GOFLAGS) qpid.apache.org/proton/event
	go test -v $(TESTFLAGS) $(GOFLAGS) qpid.apache.org/proton/messaging

$(GENERATED): genwrap.go ../../../include/proton/*.h
	go run genwrap.go


