#!/bin/bash

set -x

for file in $(find tools/tests | grep '.html$'); do
  gold=$(echo $file | sed 's/\.html/\.gold/')
  tools/html2c $file > $file.out
  diff $file.out $gold
done
