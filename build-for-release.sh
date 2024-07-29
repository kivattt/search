#!/usr/bin/env bash
bin=./bin

if [ ! -d $bin ]; then
	mkdir $bin
fi

g++ -std=c++23 -O2 -static main.cpp -o $bin/search-linux-amd64
g++ -std=c++23 -O2 -g -static main.cpp -o $bin/search-linux-amd64-debug-symbols
