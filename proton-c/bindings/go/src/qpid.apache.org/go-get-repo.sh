#!/bin/sh
#
# This script is used to create a repository to support the "go get" command.
#
# WARNING: DO NOT run in the main proton repository.
#
# This script will REPLACE the master branch of the current repository with just
# the Go subset of the proton repository.
#
# Currently the go-get repository is: https://github.com/alanconway/proton-go.git
#

set -e -x
# Safety check: the repo for `go get` should have a branch called proton_go_get_master
git checkout proton_go_get_master
git checkout master
git fetch -f https://git-wip-us.apache.org/repos/asf/qpid-proton.git master:proton_go_get_master
git checkout proton_go_get_master
git branch -f -D master  # Will replace master with the go subtree of proton
git subtree split --prefix=proton-c/bindings/go/src/qpid.apache.org -b master
git checkout master

set +x
echo
echo TO FINISH:
echo git push -f -u origin master
