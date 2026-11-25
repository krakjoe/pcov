#!/bin/sh

for i in $(find tests -type f) $(find cfg -name \*.c -o -name \*.h)
do
  k=$(basename $i)
  grep -q $k package.xml && echo $i ok || echo missing $k
done
