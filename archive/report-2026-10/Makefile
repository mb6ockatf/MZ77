# Build the coursework report: 'make' writes report.pdf.
# 'make data' regenerates data/*.dat from bench/log; 'make clean' removes
# LaTeX's intermediate files.

report.pdf: report.tex spbpu-coursework.sty chapters/*.tex data/*.dat $(wildcard pic/*)
	latexmk -pdf -interaction=nonstopmode -halt-on-error report.tex

data:
	python3 -I data/extract.py

clean:
	latexmk -C report.tex

.PHONY: data clean
