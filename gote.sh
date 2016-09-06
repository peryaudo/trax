#!/bin/sh
socat TCP:10.0.0.1:10002 exec:"./trax --client --seed=100",pty,ctty,echo=0
