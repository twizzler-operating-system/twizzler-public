
DOC_SRCS=$(addprefix doc/,overview.md us/pointers.md syscalls.md)

doc.pdf: $(DOC_SRCS) doc/style.css
	pandoc -f markdown -t html5 -o $@ --metadata pagetitle="Twizzler Documentation" -c doc/style.css --toc -V papersize:letter -s $(DOC_SRCS)
