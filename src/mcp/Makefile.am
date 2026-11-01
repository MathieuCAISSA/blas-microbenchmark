# bmb_mcp, the MCP server agents use (#262), installed beside bmb_report and
# bmb_suite in $(bindir); Python 3, standard library only.
bin_SCRIPTS = bmb_mcp
CLEANFILES = bmb_mcp

# Built rather than shipped as is: the version and where the benchmarks
# are installed are substituted, once make has expanded them. It depends
# on Makefile as well, which configure rewrites (#60).
bmb_mcp: $(srcdir)/bmb_mcp.py Makefile
	$(AM_V_GEN)sed -e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
	    -e 's|@pkglibexecdir[@]|$(pkglibexecdir)|g' \
	    $(srcdir)/bmb_mcp.py >$@.tmp && \
	chmod +x $@.tmp && mv -f $@.tmp $@

TESTS = test_bmb_mcp.sh

# test_bmb_mcp.sh checks --version against this build's version.
AM_TESTS_ENVIRONMENT = PACKAGE_VERSION='$(PACKAGE_VERSION)'; export PACKAGE_VERSION;

EXTRA_DIST = bmb_mcp.py $(TESTS)
