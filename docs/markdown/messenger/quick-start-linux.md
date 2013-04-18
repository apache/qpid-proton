Linux Proton Messenger Quick Start
==============================================


On a Linux system, these instructions take you from
zero to running your first example code.  You will 
need root privileges for one of the commands.




Prerequisite Packages
---------------------------------

For a minimum build, you will need packages installed on your
box for :

        subversion
        gcc
        cmake
        libuuid-devel



Quick Start Commands
---------------------------

    svn co http://svn.apache.org/repos/asf/qpid/proton/trunk proton
    cd ./proton
    mkdir ./build
    cd ./build
    cmake ..
    make all
    # Become root and go to your build dir.
    make install
    # Stop being root.
    # Now let's see if it works.
    cd ./proton-c/examples/messenger/c
    ./recv &
    ./send
    # You're done ! ( Kill that recv process. )
    # The output you should see:

        Address: amqp://0.0.0.0
        Subject: (no subject)
        Content: "Hello World!"





Notes
----------------------------

1. If you will be editing and checking in code from this tree,
   replace the "svn co" line with this:

        svn co https://svn.apache.org/repos/asf/qpid/proton/trunk

   You must check out through https, or you will not be able to
   check in code changes from your tree.


2. The recv application in the example defaults to the same port
   as the qpid demon.  If you happen to have that demon running,
   and using the default port, the recv app above will fail.


3. If you don't have root privileges, you can still do the 
   "make install" step by setting a non-standard prefix, thus:
        cmake -DCMAKE_INSTALL_PREFIX=/my/path ..


