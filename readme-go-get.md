The go-only subtree of proton is maintained on the branch `go1` for the `go get`
command.  `go1` is special to the `go get` command, it will use that branch
rather than `master` when it is present.

Created with:

    git subtree split --prefix=go/src/qpid.apache.org -b go1

Update with:

    git checkout go1
    git pull
    git merge -s recursive -Xsubtree=go/src/qpid.apache.org master

To see the branch description: `git config branch.go1.description`

NOTE: when updating the branch, you should also visit the doc pages at
https://godoc.org/?q=qpid.apache.org and click "Refresh now" at the bottom of
the page
