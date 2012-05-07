#!/usr/bin/env python
# vim: set fileencoding=utf8 :

# размеры автомата
_width = 60
_height = 60

# вероятности правил
_p = 0.2
_q = 0.8
_r = 0.05
_theta = 0.2  # вероятность диффузии

# начальные концентрации жертв и хищников для разных кривых, от 0 до 1
_concentrations = [ (0.1, 0.01),
	(0.8, 0.01),
	(0.8, 0.4)
]

_time_steps = 500 # количество точек на графике
_dsteps = 10 # количество шагов симуляции между двумя точками на графике (чем меньше, тем плавнее)

################ ДАЛЬШЕ НЕ ТРОГАТЬ #########################

from xml.dom.minidom import *
import subprocess
import matplotlib.pyplot as plt
import os


def setup_automaton(filename, width, height, omega, a, b, c, d, theta, p, q):
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

	prey = newdoc.createElement("state")
	prey.setAttribute("name", "x")
	automaton.appendChild(prey)

	predator = newdoc.createElement("state")
	predator.setAttribute("name", "y")
	automaton.appendChild(predator)

	# Rules
	#
	rule11 = newdoc.createElement("rule")
	rule11.setAttribute("oldstate", ".x")
	rule11.setAttribute("newstate", "xx")
	rule11.setAttribute("probability", str(a))
	automaton.appendChild(rule11)

	rule12 = newdoc.createElement("rule")
	rule12.setAttribute("oldstate", "xy")
	rule12.setAttribute("newstate", "yy")
	rule12.setAttribute("probability", str(b))
	automaton.appendChild(rule12)

	rule21 = newdoc.createElement("rule")
	rule21.setAttribute("oldstate", "y")
	rule21.setAttribute("newstate", ".")
	rule21.setAttribute("probability", str(c))
	automaton.appendChild(rule21)

#	rule22 = newdoc.createElement("rule")
#	rule22.setAttribute("oldstate", "*y")
#	rule22.setAttribute("newstate", "yy")
#	rule22.setAttribute("probability", str(d))
#	automaton.appendChild(rule22)

	diffusion1 = newdoc.createElement("rule")
	diffusion1.setAttribute("oldstate", "x.")
	diffusion1.setAttribute("newstate", ".x")
	diffusion1.setAttribute("probability", str(theta))
	automaton.appendChild(diffusion1)

	diffusion2 = newdoc.createElement("rule")
	diffusion2.setAttribute("oldstate", ".x")
	diffusion2.setAttribute("newstate", "x.")
	diffusion2.setAttribute("probability", str(theta))
	automaton.appendChild(diffusion2)

	diffusion3 = newdoc.createElement("rule")
	diffusion3.setAttribute("oldstate", "y.")
	diffusion3.setAttribute("newstate", ".y")
	diffusion3.setAttribute("probability", str(theta))
	automaton.appendChild(diffusion3)

	diffusion4 = newdoc.createElement("rule")
	diffusion4.setAttribute("oldstate", ".y")
	diffusion4.setAttribute("newstate", "y.")
	diffusion4.setAttribute("probability", str(theta))
	automaton.appendChild(diffusion4)

	diffusion5 = newdoc.createElement("rule")
	diffusion5.setAttribute("oldstate", "xy")
	diffusion5.setAttribute("newstate", "yx")
	diffusion5.setAttribute("probability", str(theta))
	automaton.appendChild(diffusion5)

	diffusion6 = newdoc.createElement("rule")
	diffusion6.setAttribute("oldstate", "yx")
	diffusion6.setAttribute("newstate", "xy")
	diffusion6.setAttribute("probability", str(theta))
	automaton.appendChild(diffusion6)

	# Cells
	#
	preys = newdoc.createElement("box")
	preys.setAttribute("x", "0")
	preys.setAttribute("y", "0")
	preys.setAttribute("height", str(height))
	preys.setAttribute("width", str(width))
	preys.setAttribute("probability", str(p))
	preys.setAttribute("state", "x")
	automaton.appendChild(preys)

	predators = newdoc.createElement("box")
	predators.setAttribute("x", "0")
	predators.setAttribute("y", "0")
	predators.setAttribute("height", str(height))
	predators.setAttribute("width", str(width))
	predators.setAttribute("probability", str(q))
	predators.setAttribute("state", "y")
	automaton.appendChild(predators)

	with open(filename, "w") as f:
		f.write(newdoc.toxml())

def count_guys(filename):
	dom = parse(filename)
	preys = 0
	predators = 0
	for node in dom.getElementsByTagName("cell"):
		if node.getAttribute("state") == "x":
			preys += 1
		if node.getAttribute("state") == "y":
			predators += 1
	return [preys, predators]


def main():
	filename = "tmp.xml"
	for (x0, y0) in _concentrations:
		setup_automaton(filename, _width, _height, 0.5, _p, _q, _r, 0.0, _theta, x0, y0)
		points = [[x0*_width*_height, y0*_width*_height]]
		for i in range (1, _time_steps):
			try:
				if os.name == "posix":
					exe = "./cellular"
				else:
					exe = "Release\cellular.exe"
				subprocess.check_call([exe, "-c", "-C", "-i", filename, "-o", filename, "-s" + str(_dsteps)])
			except Exception as e:
				print e
				pass
			points.append(count_guys(filename))
		plt.xlabel('prey concentration')
		plt.ylabel('predator concentration')
		plt.plot([float(p[0])/(_width*_height) for p in points], [float(p[1])/(_width*_height) for p in points])
	plt.show()


if __name__ == "__main__":
	main()

