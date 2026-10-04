SUBDIRS = common level1 level2 level3

# Runs every benchmark of level{1,2,3} with --verify, so it lives above
# them: "." is built and checked after SUBDIRS.
TESTS = test_bmb_verify.sh

# test_helper.sh is sourced by every per-routine smoke test in level{1,2,3}.
EXTRA_DIST = test_helper.sh $(TESTS)
