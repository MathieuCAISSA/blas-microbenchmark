ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src man

EXTRA_DIST = README.md AGENTS.md LICENSE autogen.sh \
	CHANGELOG.md CONTRIBUTING.md SECURITY.md CITATION.cff .editorconfig \
	$(TESTS)

TESTS = test_release_files.sh test_editorconfig.sh
AM_TESTS_ENVIRONMENT = PACKAGE_VERSION='$(PACKAGE_VERSION)'; export PACKAGE_VERSION;
