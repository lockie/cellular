#!/usr/bin/env python2

from random import *

max_x = 60
max_y = 60
N = 800
state = "a"


for i in range (1, N):
	print "<cell x=\"%(x)d\" y=\"%(y)d\" state=\"%(state)s\" />" % \
	{"x": randrange(max_x), "y": randrange(max_y), "state": state}



