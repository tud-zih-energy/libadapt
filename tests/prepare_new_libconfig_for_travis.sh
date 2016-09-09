#!/bin/bash

cat << EOF | sudo tee /etc/apt/preferences > /dev/null
Package: libconfig*
Pin: release n=xenial
Pin-Priority: 990

Explanation: Uninstall or do not install any Ubuntu-originated
Explanation: package versions other than those in the oneiric release
Package: *
Pin: release n=precise
Pin-Priority: 900

Package: *
Pin: release o=Ubuntu
Pin-Priority: -10
EOF

cat << EOF | sudo tee /etc/apt/sources.list.d/xenial.list > /dev/null
deb http://de.archive.ubuntu.com/ubuntu xenial main restricted universe multiverse
#deb-src http://de.archive.ubuntu.com/ubuntu xenial main restricted universe multiverse

deb http://de.archive.ubuntu.com/ubuntu xenial-updates main restricted universe multiverse
#deb-src http://de.archive.ubuntu.com/ubuntu xenial-updates main restricted universe multiverse

deb http://de.archive.ubuntu.com/ubuntu xenial-security main restricted universe multiverse
#deb-src http://de.archive.ubuntu.com/ubuntu xenial-security main restricted universe multiverse

deb http://de.archive.ubuntu.com/ubuntu xenial-backports main restricted universe multiverse
#deb-src http://de.archive.ubuntu.com/ubuntu xenial-backports main restricted universe multiverse
EOF
