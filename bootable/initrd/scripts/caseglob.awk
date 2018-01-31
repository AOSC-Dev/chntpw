#!/bin/awk -f
# awk program to make all characters in a string to glob pattern
# to match all characters both in upper and lowercase

# Example:
# foo/bar -> [Ff][Oo][Oo]/[Bb][Aa][Rr]

# (c) Petter Nordahl-Hagen, 2008

BEGIN {
  for (num=1; num < ARGC; num++) {
    s=ARGV[num];
    l=length(s);
    for (i=1; i <= l; i++) {
      c=substr(s, i, 1);
      u=toupper(c);
      k=tolower(c);
      if (u != k) {
        printf("[%c%c]",u,k);
      } else {
        printf("%c",c);
      }
    }
  } 
}


