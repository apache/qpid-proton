The go-only subtree of proton is maintained on the branch `go1` for the `go get` command.
`go1` is special to the `go get` command, it will use that branch rather than `master`
when it is present.

Created with: `git subtree split --prefix=proton-c/bindings/go/src/qpid.apache.org -b go1`
Update with:  `git checkout go1; git merge -s subtree master`

To see the branch description: `git config branch.go1.description`
