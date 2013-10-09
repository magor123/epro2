#!/bin/sh

# Create the echobox device file choosing 32 as Major number
mknod /dev/echobox c 32 1

# Give everyone read and write grants on the new driver
chmod a+r+w /dev/echobox
