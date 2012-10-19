#!/bin/bash
#
# release.sh - Creates release tarballs from the upstream source
# repository.
#

ME=$(basename ${0})
CURRDIR=$PWD
die()
{
    printf "ERROR: %s\n", "$*"
    exit 1
}

URL="http://svn.apache.org/repos/asf/qpid/proton"
BRANCH="trunk"
VERSION=""

usage()
{
    echo "Usage: ${ME} -v VERSION [-u URL] [-b BRANCH] [-c]"
    echo "-v VERSION  Specifies the release version; i.e., 0.18"
    echo "-u URL      The base URL for the repository (def. ${URL})"
    echo "-b BRANCH   The branch to check out (def. ${BRANCH})"
    echo ""
    exit 0
}


while getopts "hu:b:v:" OPTION; do
    case $OPTION in
        h) usage;;

        v) VERSION=$OPTARG;;

        u) URL=$OPTARG;;

        b) BRANCH=$OPTARG;;

        \?) usage;;
    esac
done

if [[ -z "${VERSION}" ]]; then
    die "You need to specify a version."
fi

##
## Create the C Tarball
##
rootname="qpid-proton-c-${VERSION}"
WORKDIR=$(mktemp -d)
mkdir -p "${WORKDIR}"
(
    cd ${WORKDIR}
    svn export ${URL}/${BRANCH} ${WORKDIR}/${rootname} >/dev/null

    ##
    ## Remove content not for the C tarball
    ##
    rm -f  ${rootname}/.gitignore
    rm -f  ${rootname}/config.sh
    rm -rf ${rootname}/bin
    rm -rf ${rootname}/examples/broker
    rm -rf ${rootname}/examples/mailbox
    rm -rf ${rootname}/proton-j
    rm -rf ${rootname}/design

    echo "Generating Archive: ${CURRDIR}/${rootname}.tar.gz"
    tar zcf ${CURRDIR}/${rootname}.tar.gz ${rootname}
)

##
## Create the Java Tarball
##
rootname="qpid-proton-java-${VERSION}"
WORKDIR=$(mktemp -d)
mkdir -p "${WORKDIR}"
(
    cd ${WORKDIR}
    svn export ${URL}/${BRANCH} ${WORKDIR}/${rootname} >/dev/null

    ##
    ## Remove content not for the Java tarball
    ##
    rm -f  ${rootname}/.gitignore
    rm -f  ${rootname}/config.sh
    rm -rf ${rootname}/bin
    rm -rf ${rootname}/examples
    rm -rf ${rootname}/proton-c
    rm -rf ${rootname}/design

    echo "Generating Archive: ${CURRDIR}/${rootname}.tar.gz"
    tar zcf ${CURRDIR}/${rootname}.tar.gz ${rootname}
)
