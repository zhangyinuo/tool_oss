#!/bin/sh

sh stop.sh

ulimit -c 1024000

./vfs_master
