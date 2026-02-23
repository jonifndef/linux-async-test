#!/bin/bash

mkdir -p build
gcc -o build/blocking blocking.c
gcc -o build/epoll epoll.c
gcc -o build/io_uring io_uring.c -luring
