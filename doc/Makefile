SUBDIR = v6.0
SUBVER = V6_0
MANDIR = /p/condor/public/html/manual/$(SUBDIR)
TARGETDIR = $(MANDIR).new
all:  ref.dvi
#all:  man.ps

ref.pdf:
	rm -f timesfont.tex
	echo "\usepackage{times}" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	ln -s regularfont.tex fontsize.tex
	ln -s symbol-postscript.tex symbol.tex
	make clean
	make figures
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex ref.tex
	rm -f ref.ps
	dvips ref.dvi
	rm -f ref.pdf
	gs -sDEVICE=pdfwrite -sOutputFile=ref.pdf -dNOPAUSE -dBATCH ref.ps
	rm -f ref.dvi
	rm -f ref.ps

release-pdf: ref.pdf $(TARGETDIR)
	cp ref.pdf $(TARGETDIR)/condor-$(SUBVER)-Manual.pdf


ref.dvi: 
	rm -f timesfont.tex
	echo "% do not want times" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	ln -s smallfont.tex fontsize.tex
	ln -s symbol-postscript.tex symbol.tex
	make clean
	make figures
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex ref.tex

ref.ps: ref.dvi
	dvips ref.dvi

release-ps: ref.ps $(TARGETDIR)
	cp ref.ps $(TARGETDIR)/ref.ps
	cp $(TARGETDIR)/ref.ps $(TARGETDIR)/condor-$(SUBVER)-Manual.ps
	compress $(TARGETDIR)/condor-$(SUBVER)-Manual.ps
	cp $(TARGETDIR)/ref.ps $(TARGETDIR)/condor-$(SUBVER)-Manual.ps
	gzip $(TARGETDIR)/condor-$(SUBVER)-Manual.ps
	mv $(TARGETDIR)/ref.ps $(TARGETDIR)/condor-$(SUBVER)-Manual.ps

ref.html: $(TARGETDIR)
	rm -f timesfont.tex
	echo "% do not want times" > timesfont.tex
	rm -f fontsize.tex
	rm -f symbol.tex
	ln -s regularfont.tex fontsize.tex
	ln -s symbol-html.tex symbol.tex
	make clean
	make figures
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex ref.tex
	latex2html -address condor-admin@cs.wisc.edu -split 4 -link 2 -show_section_numbers -antialias -tmp /tmp -long_titles 3 -toc_depth 2 -local_icons ref.tex
	rm -f ref.dvi
	mv ref condor-$(SUBVER)-Manual
	tar cvf condor-$(SUBVER)-Manual.tar condor-$(SUBVER)-Manual

	gzip -c condor-$(SUBVER)-Manual.tar > condor-$(SUBVER)-Manual.tar.gz
	compress condor-$(SUBVER)-Manual.tar

	mv condor-$(SUBVER)-Manual ref.new
	mv condor-$(SUBVER)-Manual.tar.Z ref.new
	mv condor-$(SUBVER)-Manual.tar.gz ref.new
	
	mv -f ref.new/*  $(TARGETDIR)
	rmdir ref.new
	touch ref.html

release-html: ref.html $(TARGETDIR)
	rm -rf $(MANDIR).old
	mv -f $(MANDIR) $(MANDIR).old
	mv -f $(TARGETDIR) $(MANDIR)
	/p/condor/public/cgi-bin/manual/human_header_links.$(SUBDIR).pl 
	rm -f ref.html

admin-man/startd-states.eps: admin-man/startd-states.fig
	rm -f admin-man/startd-states.eps
	fig2dev -L ps -m .6 admin-man/startd-states.fig > admin-man/startd-states.eps 

admin-man/startd-activities.eps: admin-man/startd-activities.fig
	rm -f admin-man/startd-activities.eps
	fig2dev -L ps -m .6 admin-man/startd-activities.fig > admin-man/startd-activities.eps 

admin-man/pool-arch.eps: admin-man/pool-arch.fig
	rm -f admin-man/pool-arch.eps
	fig2dev -L ps -m .4 admin-man/pool-arch.fig > admin-man/pool-arch.eps 

figures: admin-man/startd-states.eps admin-man/startd-activities.eps admin-man/pool-arch.eps

$(TARGETDIR):
	mkdir $(TARGETDIR)

clean:
	rm -rf ref man user admin *.dvi *.ps *.html *.log *.aux *.toc *.pdf */*.log

reallyclean: clean
	rm -rf $(TARGETDIR)

release:
	make reallyclean
	make release-ps
	make release-pdf
	make release-html
