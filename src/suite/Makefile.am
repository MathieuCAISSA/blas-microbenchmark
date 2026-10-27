# bmb_suite, the second user-facing command, installed beside bmb_report in
# $(bindir): it runs the benchmarks, which stay under libexec (AGENTS.md).
bin_SCRIPTS = bmb_suite
CLEANFILES = bmb_suite

# Built rather than shipped as is: the version and where the benchmarks
# are installed are substituted, once make has expanded them. It depends
# on Makefile as well, which configure rewrites (#60).
bmb_suite: $(srcdir)/bmb_suite.sh Makefile
	$(AM_V_GEN)sed -e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
	    -e 's|@pkglibexecdir[@]|$(pkglibexecdir)|g' \
	    $(srcdir)/bmb_suite.sh >$@.tmp && \
	chmod +x $@.tmp && mv -f $@.tmp $@

TESTS = test_bmb_suite.sh

# test_bmb_suite.sh checks --version against this build's version.
AM_TESTS_ENVIRONMENT = PACKAGE_VERSION='$(PACKAGE_VERSION)'; export PACKAGE_VERSION;

EXTRA_DIST = bmb_suite.sh $(TESTS)
