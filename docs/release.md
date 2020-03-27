### Building a release for vote:

1. Grab a clean checkout for safety.
2. Run: "git checkout ${BRANCH}" if needed.
3. Update the versions:
  - Run: "scripts/version.sh ${VERSION}", e.g: scripts/version.sh 0.18.0
4. Commit the changes, tag them.
  - Run: "git add ."
  - Run: 'git commit -m "update versions for ${TAG}"'
  - Run: 'git tag -m "tag ${TAG}" ${TAG}', e.g: git tag -m "tag 0.18.0-rc1" 0.18.0-rc1
5. Run: "scripts/export.sh $PWD ${TAG}" to create the qpid-proton-${VERSION}.tar.gz release archive.
6. Create signature and checksum files for the archive:
  - e.g "gpg --detach-sign --armor qpid-proton-${VERSION}.tar.gz"
  - e.g "sha512sum qpid-proton-${VERSION}.tar.gz > qpid-proton-${VERSION}.tar.gz.sha512"
7. Push branch changes and tag.
  - Also update versions to the applicable snapshot version for future work on it.
8. Commit artifacts to dist dev repo in https://dist.apache.org/repos/dist/dev/qpid/proton/${TAG} dir.
9. Send vote email, provide links to dist dev repo and JIRA release notes.


### After a vote succeeds:

1. Tag the RC with the final version.
2. Add the artifacts to dist release repo:
   svn cp -m "add files for qpid-proton-${VERSION}" https://dist.apache.org/repos/dist/dev/qpid/proton/${TAG} https://dist.apache.org/repos/dist/release/qpid/proton/${VERSION}
3. Give the mirrors some time to distribute things. Can take 24hrs for good coverage.
  - Status is visible at: https://www.apache.org/mirrors/
4. Update the website with release content.
5. Send release announcement email.
6. Clean out older release(s) from release repo as appropriate.
