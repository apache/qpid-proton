`go1` is a special branch for the `go get` command, it contains just the Go subtree of proton.

Created with: `git subtree split --prefix=proton-c/bindings/go/src/qpid.apache.org -b go1`
Update with:  `git checkout go1; git merge -s subtree master`

To see the branch description: `git config branch.go1.description`

