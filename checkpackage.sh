#!/bin/sh

for i in $(find tests -type f)
do
  k=$(basename $i)
  grep -q $k package.xml || echo missing $k
done
