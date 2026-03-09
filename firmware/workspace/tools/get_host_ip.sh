#! /bin/bash

# The JLINK tools don't appear to be able to work with hostnames

# To get past that, the host IP is derived from the hostname of the host that docker provides
# It is the echoed and captured by the makefile for the nrfjprog tasks
echo $HOST_IP
