all: html man

html: lampi.html mpirun.html

man: lampi.man1 mpirun.man1

lampi.man1 mpirun.man1: lampi_man.tex
	latex2man lampi_man.tex $@

lampi.html mpirun.html: lampi_man.tex
	latex2man -H lampi_man.tex $@
