#!/bin/sh

# Scan the subnet and find our device IP.

check() {
	curl --silent --max-time 2 $1 | grep -q '^Hello, world!$' && echo $1
}

for i in {1..254}; do
	check 10.0.240.$i &
done

wait
