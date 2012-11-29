#!/bin/bash

#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
# 
#   http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

#
# release.sh - Creates release tarballs from the upstream source
# repository.
#

ME=$(basename ${0})
CURRDIR=$PWD
die()
{
    printf "ERROR: %s\n" "$*"
    exit 1
}

URL="http://svn.apache.org/repos/asf/qpid/proton"
BRANCH="trunk"
VERSION=""
REVISION=""

usage()
{
    echo "Usage: ${ME} -v VERSION [-u URL] [-b BRANCH] [-r REVISION]"
    echo "-v VERSION  Specifies the release version; e.g., 3.14"
    echo "-u URL      The base URL for the repository (def. ${URL})"
    echo "-b BRANCH   The branch to check out (def. ${BRANCH})"
    echo "-r REVISION The revision to check out (def. HEAD)"
    echo ""
    exit 0
}


while getopts "hu:b:v:" OPTION; do
    case $OPTION in
        h) usage;;

        v) VERSION=$OPTARG;;

        u) URL=$OPTARG;;

        b) BRANCH=$OPTARG;;

        r) REVISION=$OPTARG;;

        \?) usage;;
    esac
done

if [[ -z "${VERSION}" ]]; then
    die "You need to specify a version."
fi

if [[ -z "${REVISION}" ]]; then
    # grab a consistent revision to use for all exports
    REVISION=$(svn info http://svn.apache.org/repos/asf/qpid/proton | fgrep Revision: | awk '{ print $2 }')
fi

echo "Using svn revision ${REVISION} for all exports."

##
## Create the C Tarball
##
rootname="qpid-proton-c-${VERSION}"
WORKDIR=$(mktemp -d)
mkdir -p "${WORKDIR}"
(
    cd ${WORKDIR}
    svn export -qr ${REVISION} ${URL}/${BRANCH}/proton-c ${rootname}
    svn export -qr ${REVISION} ${URL}/${BRANCH}/tests ${rootname}/tests

    cat <<EOF > ${rootname}/SVN_INFO
Repo: ${URL}
Branch: ${BRANCH}
Revision: ${REVISION}
EOF

    ##
    ## Remove content not for release
    ##
    rm -rf ${rootname}/examples/mailbox

    echo "Generating Archive: ${CURRDIR}/${rootname}.tar.gz"
    tar zcf ${CURRDIR}/${rootname}.tar.gz ${rootname}
)

##
## Create the Java Tarball
##
rootname="qpid-proton-j-${VERSION}"
WORKDIR=$(mktemp -d)
mkdir -p "${WORKDIR}"
(
    cd ${WORKDIR}
    svn export -qr ${REVISION} ${URL}/${BRANCH}/proton-j ${rootname}
    svn export -qr ${REVISION} ${URL}/${BRANCH}/tests ${rootname}/tests

    cat <<EOF > ${rootname}/SVN_INFO
Repo: ${URL}
Branch: ${BRANCH}
Revision: ${REVISION}
EOF

    mvn org.codehaus.mojo:versions-maven-plugin:1.2:set org.codehaus.mojo:versions-maven-plugin:1.2:commit -DnewVersion="${VERSION}" -f ${WORKDIR}/${rootname}/pom.xml

    echo "Generating Archive: ${CURRDIR}/${rootname}.tar.gz"
    tar zcf ${CURRDIR}/${rootname}.tar.gz ${rootname}
)

##
## Create the Perl Tarball
##
rootname="perl-qpid-proton-${VERSION}"
WORKDIR=$(mktemp -d)
mkdir -p "${WORKDIR}"
(
    cd ${WORKDIR}
    svn export ${URL}/${BRANCH}/proton-c/bindings/perl ${WORKDIR}/${rootname} >/dev/null

    echo "Generating Archive: ${CURRDIR}/${rootname}.tar.gz"
    perl Makefile.PL
    make dist

    mv qpid-perl-${VERSION}.tar.gz ${CURRDIR}
)

