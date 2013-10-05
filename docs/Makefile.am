if WITH_DOCS

hpxdocdir=$(top_srcdir)/docs/man/man3/

dist_man3_MANS = $(hpxdocdir)/hpx_parcel_t.3
$(hpxdocdir)/hpx_parcel_t.3: doxyfile.stamp

doxyfile.stamp:
	$(DOXYGEN) Doxyfile
	echo Timestamp > doxyfile.stamp

CLEANFILES += doxyfile.stamp

all-local: doxyfile.stamp
dist-clean:
clean-local:
	rm -rf $(top_srcdir)/docs/man/

endif
