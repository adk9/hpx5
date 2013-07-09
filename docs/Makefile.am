mandir=$(top_srcdir)/docs/man/man3/

dist_man_MANS = $(mandir)/man_page_1.3 $(mandir)/man_page_2.3
$(mandir)/man_page_1.3: doxyfile.stamp
$(mandir)/man_page_2.3: doxyfile.stamp

doxyfile.stamp:
	$(DOXYGEN) Doxyfile
	echo Timestamp > doxyfile.stamp

CLEANFILES = doxyfile.stamp

all-local: doxyfile.stamp
clean-local:
	rm -rf $(top_srcdir)/docs/man
