ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src man

EXTRA_DIST = README.md AGENTS.md LICENSE autogen.sh \
	CHANGELOG.md CONTRIBUTING.md SECURITY.md CITATION.cff .editorconfig \
	doc/dev/benchmarks.md doc/dev/backends.md doc/dev/report.md \
	doc/dev/documentation.md doc/dev/ci.md doc/dev/release.md \
	$(TESTS)

TESTS = test_release_files.sh test_editorconfig.sh test_doc_links.sh \
	test_actions_pinned.sh
AM_TESTS_ENVIRONMENT = PACKAGE_VERSION='$(PACKAGE_VERSION)'; export PACKAGE_VERSION;
