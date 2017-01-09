### Building a release for vote:

1. Grab a clean checkout for safety.
2. Run: "git checkout ${BRANCH}" to switch to a branch of the intended release point.
3. Update the versions:
  - Run: "bin/version.sh ${VERSION}", e.g. bin/version.sh 0.17.0
  - Update the version(s) if needed in file: proton-c/bindings/python/docs/conf.py
4. Commit the changes, tag them.
  - Run: "git add ."
  - Run: 'git commit -m "update versions for ${TAG}"'
  - Run: 'git tag -m "tag $TAG" $TAG'
  - Push changes. Optionally save this bit for later.
5. Run: "bin/export.sh $PWD ${TAG}" to create the qpid-proton-${TAG}.tar.gz release archive.
6. Rename and create signature and checksums for the archive:
  - e.g "mv qpid-proton-${TAG}.tar.gz qpid-proton-${VERSION}.tar.gz"
  - e.g "gpg --detach-sign --armor qpid-proton-${VERSION}.tar.gz"
  - e.g "sha1sum qpid-proton-${VERSION}.tar.gz > qpid-proton-${VERSION}.tar.gz.sha1"
  - e.g "md5sum qpid-proton-${VERSION}.tar.gz > qpid-proton-${VERSION}.tar.gz.md5"
7. Commit artifacts to dist dev repo in https://dist.apache.org/repos/dist/dev/qpid/proton/${TAG} dir.
8. Send email, provide links to dist dev repo.


### After a vote succeeds:

1. Bump the master/branch version to next 0.x.y-SNAPSHOT if it wasnt already.
2. Tag the RC with the final name/version.
3. Commit the artifacts to dist release repo in https://dist.apache.org/repos/dist/release/qpid/proton/${RELEASE} dir:
4. Update the 'latest' link in https://dist.apache.org/repos/dist/release/qpid/proton/.
5. Give the mirrors some time to distribute things.
6. Update the website with release content.
7. Send release announcement email.
