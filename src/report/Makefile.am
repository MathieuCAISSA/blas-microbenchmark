# bmb_report is the one user-facing command this project puts on $PATH:
# bin_SCRIPTS installs it to $(bindir), which follows --prefix (or --bindir
# when given) and honours DESTDIR for staged installs. The benchmarks
# themselves stay under libexec; see AGENTS.md for why.
bin_SCRIPTS = bmb_report
CLEANFILES = bmb_report

# Built rather than shipped as is: the HTML template is inserted into the
# script, so the installed command is one self-contained file with nothing
# to find at run time.
bmb_report: $(srcdir)/bmb_report.sh $(srcdir)/report.html $(srcdir)/assemble.awk
	$(AM_V_GEN)$(AWK) -v version='$(PACKAGE_VERSION)' -f $(srcdir)/assemble.awk \
	    $(srcdir)/report.html $(srcdir)/bmb_report.sh >$@.tmp && \
	chmod +x $@.tmp && mv -f $@.tmp $@

TESTS = test_bmb_report.sh

FIXTURES = \
	fixtures/README \
	fixtures/not-ours.json \
	fixtures/v0.5.0.json \
	fixtures/v0.6.1.json \
	fixtures/v2.0.0.json

EXTRA_DIST = bmb_report.sh report.html assemble.awk $(TESTS) $(FIXTURES)
