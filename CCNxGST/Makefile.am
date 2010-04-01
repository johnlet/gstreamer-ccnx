SUBDIRS = m4 CCNxPlug

docpkg = $(PACKAGE_TARNAME)-doxy-$(PACKAGE_VERSION).tar.gz

doc_DATA = $(docpkg)

$(docpkg): doxygen.stamp
	tar chof - doc/html | gzip -9 -c >$@

doxygen.stamp: Doxyfile
	$(DOXYGEN) $(DOXYFLAGS) $<
	echo Timestamp >$@

CLEANFILES = doxywarn.txt doxygen.stamp $(docpkg)

clean-local:
	-rm -rf doc/html


EXTRA_DIST = autogen.sh gst-autogen.sh
