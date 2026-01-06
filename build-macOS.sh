#!/bin/bash

cd "$(dirname "$0")"

rm -f cacheline_detect 

make

echo ''

./cacheline_detect
