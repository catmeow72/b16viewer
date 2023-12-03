.PHONY: dist
dist: $(PROGRAM)
	cp $(PROGRAM) dist/BMXVIEW.PRG
	rm -f dist.zip
	zip dist.zip $(wildcard dist/*)
