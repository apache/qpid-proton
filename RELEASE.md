###Building a release for vote:

1. git checkout <branch> where <branch> is whatever branch you intend to release from
  - this *should* be able to be any branch, ref, tag, etc
2. run bin/release.sh <VERSION> <TAG>, e.g. bin/release.sh 0.9 0.9-rc1
3. run git push origin ... as described # when exactly should this happen?
4. cd into some scratch directory
5. run ~/proton/bin/export.sh
6. run sign qpid-proton-...
7. run tar -xzf qpid-proton-...
8. cd qpid-proton-...
9. mvn deploy -Papache-release -DskipTests=true
10. https://repository.apache.org/index.html#stagingProfiles
11. close the repo...
12. upload/send email

###After a vote succeeds:

1. bump the master/branch version to 0.x-SNAPSHOT
2. tag rc with final name
3. rename artifacts
4. commit artifacts to qpid-dist
5. update latest link
6. release java stuff from nexus
7. update qpid-site with release content
8. update development roadmap
