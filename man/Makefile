all: html man

html: lampi_man.html

man: lampi.man1 mpirun.man1

lampi.man1 mpirun.man1: lampi_man.tex
	latex2man lampi_man.tex $@

lampi_man.html: lampi_man.tex
	latex2man -H lampi_man.tex $@
