#!/usr/bin/env python

import sys

stack = []
time_stack = []

TS=7
TT=8
TYPE = 2
NAME = 4

def process(start, end):
    toptime = time_stack.pop()
    time = int(end[TS]) - int(start[TS])
    tt = int(end[TT]) - int(start[TT])
    elap = ((time - tt) / 4) - toptime
    for s in stack:
        print(s[NAME] + ";", end='')
    print(end[NAME] + " " + str(elap))# + " " + str((time - tt)/4))
    return elap

for line in sys.stdin:
    line = line.rstrip().strip()
    item = line.split()
 #   print(item)
    if len(item) < 10:
        continue
    if item[0] != '{':
        continue
    if item[TYPE] == 'enter':
        stack.append( item )
        time_stack.append(0)
    else:
        last = stack[-1]
        if last[NAME] == item[NAME]:
            time = process(stack.pop(), item)
            if len(time_stack) > 0:
                toptime = time_stack.pop()
                time_stack.append(time + toptime)
