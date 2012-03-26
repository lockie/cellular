#!/usr/bin/env python
# vim: set fileencoding=utf8 :


width  = 100
height = 100
# общий размер поля (>= 25x25)

# параметр \omega модели КА
omega  = 0.5
# вероятность диффузии
r = 0.5
# начальная концентрация в первой половине нейрона
x0 = 0.5

# число точек по координате t на итоговом графике
time_steps = 10
# количество шагов автомата между двумя точками на графике
dsteps = 5
# число прогонов автомата для усреднения
averaging = 5
# шаг по параметрам p и q, для которых проводить вычисления
param_step = 0.1


############################
# NO CHANGE BEYOND THIS LINE
############################


import time
from random import *
from xml.dom.minidom import *
import subprocess
import numpy
import matplotlib.pyplot as plt


def create_automaton(p, q, r, x0, filename):
	newdoc = getDOMImplementation().createDocument(None, "automaton", None)
	automaton = newdoc.documentElement
	automaton.setAttribute("width", str(width))
	automaton.setAttribute("height", str(height))
	automaton.setAttribute("omega", str(omega))

	# States
	#
	zerostate = newdoc.createElement("zerostate")
	zerostate.setAttribute("name", ".")
	automaton.appendChild(zerostate)

	neurotransmitter = newdoc.createElement("state")
	neurotransmitter.setAttribute("name", "x")
	automaton.appendChild(neurotransmitter)

	membrane = newdoc.createElement("state")
	membrane.setAttribute("name", "m")
	automaton.appendChild(membrane)

	# Rules
	#
	rule1 = newdoc.createElement("rule")
	rule1.setAttribute("oldstate", "xx.")
	rule1.setAttribute("newstate", "xxx")
	rule1.setAttribute("probability", str(p))
	automaton.appendChild(rule1)

	rule2 = newdoc.createElement("rule")
	rule2.setAttribute("oldstate", "x.")
	rule2.setAttribute("newstate", "..")
	rule2.setAttribute("probability", str(q))
	automaton.appendChild(rule2)

	diffusion = newdoc.createElement("rule")
	diffusion.setAttribute("oldstate", "x.")
	diffusion.setAttribute("newstate", ".x")
	diffusion.setAttribute("probability", str(r))
	automaton.appendChild(diffusion)

	diffusion1 = newdoc.createElement("rule")
	diffusion1.setAttribute("oldstate", ".x")
	diffusion1.setAttribute("newstate", "x.")
	diffusion1.setAttribute("probability", str(r))
	automaton.appendChild(diffusion1)

	# Cells
	#
	neuron_top = newdoc.createElement("box")
	neuron_top.setAttribute("x", "10")
	neuron_top.setAttribute("y", "10")
	neuron_top.setAttribute("height", "1")
	neuron_top.setAttribute("width", str(width-20))
	neuron_top.setAttribute("state", "m")
	automaton.appendChild(neuron_top)

	neuron_bottom = newdoc.createElement("box")
	neuron_bottom.setAttribute("x", "10")
	neuron_bottom.setAttribute("y", str(height-10))
	neuron_bottom.setAttribute("height", "1")
	neuron_bottom.setAttribute("width", str(width-20))
	neuron_bottom.setAttribute("state", "m")
	automaton.appendChild(neuron_bottom)

	neuron_left = newdoc.createElement("box")
	neuron_left.setAttribute("x", "10")
	neuron_left.setAttribute("y", "10")
	neuron_left.setAttribute("height", str(height-20))
	neuron_left.setAttribute("width", "1")
	neuron_left.setAttribute("state", "m")
	automaton.appendChild(neuron_left)

	neuron_right = newdoc.createElement("box")
	neuron_right.setAttribute("x", str(width-10))
	neuron_right.setAttribute("y", "10")
	neuron_right.setAttribute("height", str(height-19))
	neuron_right.setAttribute("width", "1")
	neuron_right.setAttribute("state", "m")
	automaton.appendChild(neuron_right)

	impulse = newdoc.createElement("box")
	impulse.setAttribute("x", "11")
	impulse.setAttribute("y", "11")
	impulse.setAttribute("height", str(height-21))
	impulse.setAttribute("width", str(width/2 - 10))
	impulse.setAttribute("probability", str(x0))
	impulse.setAttribute("state", "x")
	automaton.appendChild(impulse)

	with open(filename, "w") as f:
		f.write(newdoc.toxml())


def count_concentration(filename):
	dom = parse(filename)
	count = 0
	for node in dom.getElementsByTagName("cell"):
		if node.getAttribute("state") == "x":
			x = int(node.getAttribute("x"))
			if x > width / 2:
				count += 1
	return float(count) / ((width-20)*(height-20)/2)


def run_oneshot(p, q, r, x0):
	filename = "/tmp/test.xml"
	create_automaton(p, q, r, x0, filename)
	points = []
	for i in range(1, time_steps):
		try:
			subprocess.check_call(["./cellular", "-i", filename, "-c", "-o", filename, "-s" + str(dsteps)])
		except:
			pass
		points.append(count_concentration(filename))
	return points


def walk_q(p, r, x0):
	for q in numpy.arange(0, r+0.1, param_step): # вероятность второго ур-я
		if r+q >= 1:
			break
		points = [0 for i in range(1, time_steps)]
		for i in range(1, averaging):
			points = map(lambda x, y: x+y, points, run_oneshot(p, q, r, x0))
		points = map(lambda x: x/averaging, points)
		plt.plot([t*dsteps for t in range(1, time_steps)], points, label="q="+str(q))


def walk_p(r, x0):
	counter = 0
	for p in numpy.arange(0, r+0.1, param_step): # вероятность первого ур-я
		plt.figure(counter)
		counter += 1
		walk_q(p, r, x0)
		plt.title("p="+str(p))
		plt.xlabel("time step")
		plt.ylabel("impulse cells concentration")
		plt.legend()
		plt.savefig("p="+str(p)+".png")


def main():
	t0 = time.clock()
	walk_p(r, x0)
	print "Processing took", time.clock() - t0, "s"
	return 0


if __name__ == "__main__":
	main()

