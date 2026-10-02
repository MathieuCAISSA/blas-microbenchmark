# Builds bmb_report from report.html and bmb_report.sh:
#
#     awk -v version=1.0.0 -f assemble.awk report.html bmb_report.sh > bmb_report
#
# The template is split at its "<!-- @BMB_RESULTS@ -->" line, where the
# results go: what precedes it replaces @BMB_REPORT_HEAD@ in the script,
# what follows replaces @BMB_REPORT_TAIL@. Both land inside *quoted*
# here-documents, so the template is copied byte for byte -- no shell
# expansion, no substitution -- which is also why it must not contain a
# line equal to either delimiter: that line would end the here-document.

function fail(msg) {
    print "assemble.awk: " msg | "cat 1>&2"
    failed = 1
    exit 1
}

BEGIN {
    split(version, v, ".")
    accept = v[1]
    if (accept !~ /^[0-9]+$/) {
        fail("cannot take a major version from \"" version "\"")
    }
}

FNR == 1 {
    file++
}

file == 1 {
    if ($0 == "BMB_REPORT_HEAD" || $0 == "BMB_REPORT_TAIL") {
        fail("report.html line " FNR " would end a here-document early")
    }
    if ($0 == "<!-- @BMB_RESULTS@ -->") {
        if (split_at) {
            fail("report.html has the results marker twice")
        }
        split_at = FNR
    }
    template[FNR] = $0
    lines = FNR
    next
}

file == 2 {
    if (!split_at) {
        fail("report.html has no <!-- @BMB_RESULTS@ --> line")
    }
    if ($0 == "@BMB_REPORT_HEAD@") {
        for (i = 1; i < split_at; i++) {
            print template[i]
        }
        next
    }
    if ($0 == "@BMB_REPORT_TAIL@") {
        for (i = split_at + 1; i <= lines; i++) {
            print template[i]
        }
        next
    }
    gsub(/@PACKAGE_VERSION@/, version)
    gsub(/@ACCEPT_MAJOR@/, accept)
    print
}

END {
    if (failed) {
        exit 1
    }
}
