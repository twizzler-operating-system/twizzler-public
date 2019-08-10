
DOC_SRCS=$(addprefix doc/,overview.md us/pointers.md syscalls.md)

TEXOPTS=--variable mainfont="Source Sans Pro" --variable sansfont="Source Sans Pro" --variable fontsize=10pt --variable version=2.0

doc.pdf: $(DOC_SRCS) doc/style.css doc/include.mk doc/template.tex doc/vars.yaml
	pandoc -f markdown -o $@ --template=doc/template.tex --metadata pagetitle="Twizzler Documentation" -c doc/style.css --toc -V papersize:letter -s --toc-depth 2 --pdf-engine=xelatex --metadata-file=doc/vars.yaml $(TEXOPTS) $(DOC_SRCS)
