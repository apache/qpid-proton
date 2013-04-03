Linux Quick Start
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

    mkdir ~/proton
    cd ~/proton
    svn co http://svn.apache.org/repos/asf/qpid/proton/trunk
    cd ./trunk
    mkdir ./build
    cd ./build
    cmake -DCMAKE_INSTALL_PREFIX=/usr ..
    make all
    # Become root and go to your build dir.
    make install
    # Stop being root.
    # Now let's see if it works.
    cd ~/proton/trunk/examples/messenger/c
    cmake .
    make
    ./recv &
    ./send
    # You're done !
    # The output you should see:

        Address: amqp://0.0.0.0
        Subject: (no subject)
        Content: "Hello World!"

