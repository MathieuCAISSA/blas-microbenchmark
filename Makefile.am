ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src man

EXTRA_DIST = README.md AGENTS.md LICENSE autogen.sh \
	CHANGELOG.md CONTRIBUTING.md \
	.github/CODE_OF_CONDUCT.md .github/SECURITY.md .github/REVIEWS.md \
	CITATION.cff .editorconfig \
	doc/dev/benchmarks.md doc/dev/backends.md doc/dev/report.md \
	doc/dev/documentation.md doc/dev/ci.md doc/dev/release.md doc/dev/packaging.md \
	$(TESTS)

TESTS = tests/test_release_files.sh tests/test_editorconfig.sh \
	tests/test_doc_links.sh tests/test_actions_pinned.sh tests/test_configure_paths.sh
AM_TESTS_ENVIRONMENT = PACKAGE_VERSION='$(PACKAGE_VERSION)'; export PACKAGE_VERSION;
