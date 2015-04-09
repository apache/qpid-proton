# FIXME aconway 2015-04-09: integrate with cmake

#GOFLAGS=-compiler gccgo -gccgoflags "-g -O0"
#GOFLAGS=-gcflags "-N -l"

GENERATED=qpid.apache.org/proton/event/wrappers_gen.go

test: $(GENERATED)
	go test $(GOFLAGS) qpid.apache.org/proton
	go test $(GOFLAGS) qpid.apache.org/proton/event

rebuild: $(GENERATED)
	go build -a $(GOFLAGS) qpid.apache.org/proton
	go build -a $(GOFLAGS) qpid.apache.org/proton/event
	go test $(GOFLAGS) -c -a qpid.apache.org/proton
	go test $(GOFLAGS) -c -a qpid.apache.org/proton/event

qpid.apache.org/proton/event/wrappers_gen.go: genwrap.go ../../../include/proton/*.h
	go run genwrap.go

broker: test
	go build $(GOFLAGS) ~/proton/examples/go/event/broker.go
	./broker


