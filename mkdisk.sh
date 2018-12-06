#! /bin/bash
dd if=/dev/zer of=$1 bs=1024 count=1440
mkfs -b 1024 $1 1440