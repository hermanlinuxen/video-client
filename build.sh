#!/bin/bash

outfile=video-client
opts='-lcurl'

if [ ! $outfile ]; then echo "No outfile provided..."; exit 1; fi

g++ ./main.cpp $opts -o $outfile
