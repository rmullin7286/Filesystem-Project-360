#! /bin/bash
dd if=/dev/zer of=mydisk bs=1024 count=1440
mkfs -b 1024 mydisk 1440