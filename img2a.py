#!/usr/bin/env python
# vim: set fileencoding=utf8 :

from PIL import Image
import sys
from xml.dom.minidom import *


def main():
	if not len(sys.argv) == 3:
		print "Usage:", sys.argv[0], "<image file name> <automaton file name>"
		sys.exit(1)

	img = Image.open(sys.argv[1])
	pix = img.load()

	document = getDOMImplementation().createDocument(None, "automaton", None)
	automaton = document.documentElement
	automaton.setAttribute("width", str(img.size[0]))
	automaton.setAttribute("height", str(img.size[1]))

	pixels = [] # all different kinds of pixels
	cells = []

	for y in range(img.size[1]):
		for x in range(img.size[0]):
			px = pix[x, y]
			if not px in pixels:
				pixels.append(px)
			cell = str(pixels.index(px))
			if not pixels.index(px) == 0: # assume pixel at (0, 0) is zerostate
				c = document.createElement("cell")
				c.setAttribute("x", str(x))
				c.setAttribute("y", str(y))
				c.setAttribute("state", cell)
				cells.append(c)

	zerostate = document.createElement("zerostate")
	zerostate.setAttribute("name", "0")
	automaton.appendChild(zerostate)

	for s in range(1, len(pixels)):
		state = document.createElement("state")
		state.setAttribute("name", str(s))
		automaton.appendChild(state)

	for c in cells:
		automaton.appendChild(c)

	with open(sys.argv[2], "w") as f:
		f.write(document.toprettyxml())


if __name__ == "__main__":
	main()

