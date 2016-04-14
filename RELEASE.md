### Building a release for vote:

1. Grab a clean checkout for safety.
2. Run: "git checkout ${BRANCH}" to switch to a branch of the intended release point.
3. Update the versions:
  - Run: "bin/version.sh ${VERSION}", e.g. bin/release.sh 0.12.2.
  - Update the version if needed in file: proton-c/bindings/python/setuputils/bundle.py
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
7. Deploy the Java binaries to a staging repo:
  - Run: "tar -xzf qpid-proton-${VERSION}.tar.gz"
  - Run: "cd qpid-proton-${VERSION}"
  - Run: "mvn deploy -Papache-release -DskipTests=true"
8. Close the staging repo:
  - Log in at https://repository.apache.org/index.html#stagingRepositories
  - Find the new 'open' staging repo just created and select its checkbox.
  - Click the 'close' button, provide a description, e.g "Proton ${TAG}" and close the repo.
  - Wait a few seconds, hit the 'refresh' button and confirm the repo is now 'closed'.
  - Click on the repo and find its URL listed in the summary.
9. Commit artifacts to dist dev repo in https://dist.apache.org/repos/dist/dev/qpid/proton/${TAG} dir.
10. Send email, provide links to dist dev repo and the staging repo.


### After a vote succeeds:

1. Bump the master/branch version to next 0.x.y-SNAPSHOT if it wasnt already.
2. Tag the RC with the final name/version.
3. Commit the artifacts to dist release repo in https://dist.apache.org/repos/dist/release/qpid/proton/${RELEASE} dir:
  - Rename the files to remove the RC suffix.
  - Fix filename within .sha and .md5 checksums or regenerate.
4. Update the 'latest' link in https://dist.apache.org/repos/dist/release/qpid/proton/.
5. Release the staging repo:
  - Log in at https://repository.apache.org/index.html#stagingRepositories
  - Find the 'closed' staging repo representing the RC select its checkbox.
  - Click the 'Release' button and release the repo.
6. Give the mirrors some time to distribute things.
7. Update the website with release content.
8. Update development roadmap.
9. Send release announcement email.
