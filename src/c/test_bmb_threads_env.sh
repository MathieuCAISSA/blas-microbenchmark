#!/bin/sh
# Checks how a benchmark takes its thread count from the environment, for
# the variables of the backend it was built with, in that backend's order:
# without -t the first one set to a number of threads is used; with -t, -t
# wins and a variable that disagrees is reported; a value that is not a
# number of threads (0, -2, "3 ", 2,1, ...) is ignored with a warning,
# where it used to label a run with 4294967294 threads; another backend's
# variables are not read. The thread count is read back from the table and
# the JSON, where a wrong one would end up in the report. Under MKL, a
# count per domain (MKL_DOMAIN_NUM_THREADS) is overridden with a warning.
# More threads than the CPUs the process may run on (its affinity, its
# cgroup's quota) are warned about, on a fixture.
set -e

T=threads-env-test.d
B=level1/bmb_ddot
config=${top_builddir:?top_builddir is not set}/config.h

fail() {
    echo "FAIL: $*"
    exit 1
}

ok() {
    echo "ok   $*"
}

test -x "$B" || fail "no $B"
test -f "$config" || fail "no $config"

# The variables the backend reads, in its order: the #if chain at the top
# of bmb_threads.c, from the same config.h.
defined() {
    grep -q "^#define $1 " "$config"
}
if defined BMB_NO_THREAD_CONTROL; then
    vars=
elif defined HAVE_FLEXIBLAS_API; then
    # The library FlexiBLAS loaded decides, as --version names it.
    case $("$B" --version | sed -n 's/^BLAS backend: \([^ ]*\).*/\1/p') in
        flexiblas/openblas*) vars="OPENBLAS_NUM_THREADS GOTO_NUM_THREADS OMP_NUM_THREADS" ;;
        flexiblas/blis*) vars="BLIS_NUM_THREADS OMP_NUM_THREADS" ;;
        flexiblas/netlib*) vars= ;;
        *) vars="OMP_NUM_THREADS" ;;
    esac
elif defined HAVE_MKL_DOMAIN_SET_NUM_THREADS; then
    vars="MKL_NUM_THREADS OMP_NUM_THREADS"
elif defined HAVE_BLI_THREAD_SET_NUM_THREADS; then
    vars="BLIS_NUM_THREADS OMP_NUM_THREADS"
elif defined HAVE_OPENBLAS_SET_NUM_THREADS; then
    vars="OPENBLAS_NUM_THREADS GOTO_NUM_THREADS OMP_NUM_THREADS"
elif defined HAVE_OMP_SET_NUM_THREADS; then
    vars="OMP_NUM_THREADS"
else
    vars=
fi
all="OPENBLAS_NUM_THREADS GOTO_NUM_THREADS BLIS_NUM_THREADS MKL_NUM_THREADS OMP_NUM_THREADS"
unset MKL_DOMAIN_NUM_THREADS
# Whatever the calling shell has set must not leak into the cases.
for v in $all; do
    unset "$v"
done

rm -rf "$T"
mkdir "$T"

# run OPTIONS [VAR=value ...]: one tiny run; its thread count, from the
# table and from the JSON, must agree. Values go to env untouched, spaces
# included.
run() {
    opts=$1
    shift
    # shellcheck disable=SC2086
    env "$@" "$B" -x 0 -i 1 -v 8 -C -o "$T/out.json" $opts >"$T/out.txt" 2>"$T/err" \
        || fail "$* $B $opts: exit $?: $(cat "$T/err")"
    table=$(awk '/^Thread count/ { getline; print $1; exit }' "$T/out.txt")
    json=$(sed -n 's/^ *"thread_count": \([0-9]*\),$/\1/p' "$T/out.json")
    test -n "$table" && test "$table" = "$json" \
        || fail "$*: the table says $table threads, the JSON $json"
    got=$table
}

# expect N OPTIONS [VAR=value ...]
expect() {
    want=$1
    shift
    run "$@"
    test "$got" = "$want" || fail "$*: $got threads, expected $want"
}

warned() {
    grep -qF -- "$1" "$T/err" || fail "no warning \"$1\" (stderr: $(cat "$T/err"))"
}

quiet() {
    if grep -q 'is ignored\|not a number of threads' "$T/err"; then
        fail "$1: unexpected warning: $(cat "$T/err")"
    fi
}

expect 1 ""
quiet "no variable"
ok "nothing set: 1 thread"

# More threads than the process may run on: the container fixture's
# affinity of 5 CPUs under a quota of 2.5, so 3. A backend that is given
# the thread count is warned about it; one that takes none (Netlib, also
# as FlexiBLAS's) is not, since its threads are not ours.
root=${srcdir:?srcdir is not set}/common/fixtures/machine/container
takes=yes
[ -n "$vars" ] || takes=no
cpus_warning="may only run on 3 CPUs"
expect 3 "-t 3" "BMB_MACHINE_ROOT=$root"
if grep -qF "$cpus_warning" "$T/err"; then
    fail "-t 3 on 3 usable CPUs was warned about: $(cat "$T/err")"
fi
BMB_MACHINE_ROOT=$root "$B" -x 0 -i 1 -v 8 -C -t 1,4 >/dev/null 2>"$T/err" \
    || fail "-t 1,4 on the container fixture: exit $?: $(cat "$T/err")"
if [ "$takes" = yes ]; then
    warned "The run uses up to 4 threads, but this process may only run on 3 CPUs"
    BMB_MACHINE_ROOT=$root "$B" -x 0 -i 1 -v 8 -C -t 4,1 >/dev/null 2>"$T/err" \
        || fail "-t 4,1 on the container fixture: exit $?: $(cat "$T/err")"
    warned "The run uses up to 4 threads, but this process may only run on 3 CPUs"
    # The pinned fixture: one CPU, said in the singular.
    BMB_MACHINE_ROOT=${srcdir}/common/fixtures/machine/pinned "$B" -x 0 -i 1 -v 8 -C -t 2 \
        >/dev/null 2>"$T/err" || fail "-t 2 on the pinned fixture: exit $?: $(cat "$T/err")"
    warned "may only run on 1 CPU (its CPU affinity"
    ok "-t 1,4 and -t 4,1 on 3 usable CPUs: a warning, naming the most threads; 1 CPU in the singular"
elif grep -qF "$cpus_warning" "$T/err"; then
    fail "a backend that takes no thread count was warned about CPUs: $(cat "$T/err")"
else
    ok "a backend that takes no thread count: no warning about CPUs"
fi

if [ -z "$vars" ]; then
    for v in $all; do
        expect 1 "" "$v=3"
        quiet "$v=3"
    done
    ok "this backend has no thread control: none of $all is read"
    rm -rf "$T"
    echo "all checks passed"
    exit 0
fi

for v in $vars; do
    expect 3 "" "$v=3"
    quiet "$v=3"
    expect 2 "-t 2" "$v=3"
    warned "$v is ignored! Set to 3 but option -t is set to 2."
    expect 2 "-t 2" "$v=2"
    quiet "$v=2 -t 2"
    env "$v=2" "$B" -x 0 -i 1 -v 8 -C -t 1,2 >/dev/null 2>"$T/err" \
        || fail "$v=2 -t 1,2: exit $?: $(cat "$T/err")"
    warned "$v is ignored! Set to 2 but option -t is set to 1, 2."
    expect 1 "" "$v="
    quiet "$v empty"
    for bad in 0 -2 abc '3 ' ' 3' 2,1 99999999999 2147483648 99999999999999999999; do
        expect 1 "" "$v=$bad"
        warned "$v=\"$bad\" is not a number of threads; ignored."
    done
done
ok "each of $vars: used without -t; overridden by -t with a warning when it disagrees; ignored with a warning when not a number of threads"

# The order: every earlier variable wins over every later one, and an
# invalid one gives way to the next.
for a in $vars; do
    later=false
    for b in $vars; do
        if [ "$b" = "$a" ]; then
            later=true
            continue
        fi
        $later || continue
        expect 3 "" "$a=3" "$b=5"
        quiet "$a=3 $b=5"
        expect 5 "" "$a=0" "$b=5"
        warned "$a=\"0\" is not a number of threads; ignored."
    done
done
ok "in the backend's order: $vars"

for v in $all; do
    case " $vars " in *" $v "*) continue ;; esac
    expect 1 "" "$v=3"
    quiet "$v=3"
done
ok "another backend's variables are not read"

# MKL also takes a count per domain, MKL_DOMAIN_NUM_THREADS="MKL_DOMAIN_BLAS=4",
# which the count set for its BLAS domain overrides: said, and not used.
if defined HAVE_MKL_DOMAIN_SET_NUM_THREADS; then
    expect 1 "" "MKL_DOMAIN_NUM_THREADS=MKL_DOMAIN_BLAS=4"
    warned "MKL_DOMAIN_NUM_THREADS is ignored for BLAS"
    expect 2 "-t 2" "MKL_DOMAIN_NUM_THREADS=MKL_DOMAIN_BLAS=4"
    ok "MKL_DOMAIN_NUM_THREADS is overridden, with a warning"
fi

rm -rf "$T"
echo "all checks passed"
