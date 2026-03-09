#! /bin/bash

# Commands to execute after startup

# remove HOST_IP setting from .bashrc 
sed -i '/HOST_IP/d' ~/.bashrc

# Wait until host.docker.internal is available (max 10 seconds)
for i in {1..10}; do
  if grep -q "host.docker.internal" /etc/hosts; then
    break
  fi
  sleep 1
done

HOST_IP=$(awk '/host.docker.internal/ {print $1}' /etc/hosts)
echo "export HOST_IP=${HOST_IP}" >> ~/.bashrc
source ~/.bashrc

# Set the permissions correctly on the /tmp folder
chmod 1777 /tmp 

chmod +x ../build_flash.sh

echo "Startup script completed"
