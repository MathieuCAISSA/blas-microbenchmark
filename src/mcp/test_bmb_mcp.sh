#!/bin/sh
# Checks bmb_mcp (#262), the MCP server, through its standard input as a
# client drives it: the handshake, every tool on this build tree at tiny
# sizes (the routines, the machine, a benchmark run and read back, the
# suite, the report), each refusal before anything runs (a path outside
# the results directory, more threads than CPUs, a name taken, an
# argument the tool or the benchmark does not take), a run past the
# memory limit, one past the time cap, a wrong result, and messages that
# are not requests. SKIPs without python3.
set -e

R=./bmb_mcp
T=mcp-test.d

fail() {
    echo "FAIL: $*"
    exit 1
}

command -v python3 >/dev/null 2>&1 || { echo "SKIP: no python3 to run bmb_mcp"; exit 77; }

rm -rf "$T"
mkdir "$T"

v=$($R --version)
test "$v" = "bmb_mcp ${PACKAGE_VERSION:?PACKAGE_VERSION is not set}" || fail "--version says '$v'"
$R --help | grep -q '^Usage: bmb_mcp' || fail "--help gives no usage"
if $R --nope </dev/null >/dev/null 2>"$T/err"; then
    fail "an unknown option was taken"
fi
grep -q 'unknown option: --nope' "$T/err" || fail "an unknown option: $(cat "$T/err")"
echo "ok   --version, --help, an unknown option refused"

# call ID TOOL ARGS: a tools/call request, a line.
call() {
    printf '{"jsonrpc":"2.0","id":%s,"method":"tools/call","params":{"name":"%s","arguments":%s}}\n' "$1" "$2" "$3"
}
fast='"iterations":1,"warmup_time":0'
over=$(( $(nproc) + 1 ))
{
    echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"test","version":"0"}}}'
    echo '{"jsonrpc":"2.0","method":"notifications/initialized"}'
    echo '{"jsonrpc":"2.0","id":2,"method":"tools/list"}'
    call 3 list_routines '{}'
    call 4 list_routines '{"level":[3],"precision":["d"]}'
    call 5 describe_machine '{}'
    call 6 run_benchmark "{\"routine\":\"ddot\",\"vector_size\":\"64:256\",$fast}"
    call 7 run_benchmark "{\"routine\":\"dgemm\",\"dim1\":\"16,32\",\"trans\":\"NT\",\"name\":\"gemm-nt\",$fast}"
    call 8 read_result '{"path":"gemm-nt.json"}'
    call 9 read_result '{"path":"../../../etc/passwd"}'
    call 10 run_benchmark '{"routine":"ddot","name":"../x"}'
    call 11 run_benchmark "{\"routine\":\"ddot\",\"threads\":\"$over\"}"
    call 12 run_benchmark '{"routine":"dfoo"}'
    call 13 run_benchmark '{"routine":"ddot","foo":1}'
    call 14 run_benchmark '{"routine":"ddot","vector_size":"0"}'
    call 15 run_suite "{\"name\":\"suite\",\"routine\":[\"ddot\"],$fast}"
    call 16 make_report '{}'
    call 17 run_benchmark "{\"routine\":\"dgemm\",\"dim1\":\"16\",\"name\":\"gemm-nt\",$fast}"
    call 18 no_such_tool '{}'
    echo '{"jsonrpc":"2.0","id":19,"method":"no/such"}'
    echo 'not json'
    echo '{"jsonrpc":"2.0","id":21,"method":"ping"}'
} >"$T/requests"
$R --results-dir "$T/results" <"$T/requests" >"$T/responses" 2>"$T/err" || fail "bmb_mcp failed: $(cat "$T/err")"

# A run past the memory limit, one past the time cap, and a wrong result,
# each from a server of its own. The limits are lowered rather than the
# sizes raised: were a check broken, the run would still be small.
call 1 run_benchmark "{\"routine\":\"dgemm\",\"dim1\":\"512\",$fast}" |
    $R --results-dir "$T/results" --memory-limit 1000000 >"$T/memory" 2>>"$T/err"
call 1 run_benchmark '{"routine":"ddot","vector_size":"64","iterations":1,"warmup_time":0,"min_time":5}' |
    $R --results-dir "$T/results" --time-cap 1 >"$T/time" 2>>"$T/err"
call 1 run_benchmark "{\"routine\":\"ddot\",\"vector_size\":\"64\",$fast}" |
    BMB_VERIFY_CORRUPT=1 $R --results-dir "$T/results" >"$T/wrong" 2>>"$T/err"

# Routines in the table: as many as list_routines must give.
n=$(sed -n '/^ROUTINES = """$/,/^"""$/p' "$R" | tr ' ' '\n' | grep -c ':')

python3 - "$T" "$n" "$PACKAGE_VERSION" <<'EOF'
import json, os, sys
T, routines, version = sys.argv[1], int(sys.argv[2]), sys.argv[3]
bad = []
def check(ok, what):
    if not ok:
        bad.append(what)

lines = [json.loads(l) for l in open(T + '/responses')]
by = {}
for m in lines:
    by.setdefault(m.get('id'), m)
check(len(lines) == 21, '%d responses for 20 requests and a line that is not JSON, the notification unanswered' % len(lines))
def result(i):
    return by.get(i, {}).get('result', {})
def text(i):
    return ' '.join(c.get('text', '') for c in result(i).get('content', []))
def refused(i, words):
    check(result(i).get('isError') is True and words in text(i), 'call %d: not refused with "%s": %s' % (i, words, text(i)))

init = result(1)
check(init.get('protocolVersion') == '2025-06-18', 'initialize: the protocol asked for')
check(init.get('serverInfo') == {'name': 'bmb_mcp', 'version': version}, 'initialize: serverInfo')
check('tools' in init.get('capabilities', {}), 'initialize: no tools capability')
tools = result(2).get('tools', [])
names = sorted(t['name'] for t in tools)
check(names == ['describe_machine', 'list_routines', 'make_report', 'read_result', 'run_benchmark', 'run_suite'], 'tools/list: %s' % names)
check(all(t['inputSchema'].get('type') == 'object' and t.get('description') for t in tools), 'a tool without a schema or a description')
check(len(result(3).get('structuredContent', {}).get('routines', [])) == routines, 'list_routines: not every routine')
d3 = sorted(r['routine'] for r in result(4).get('structuredContent', {}).get('routines', []))
check(d3 == ['dgemm', 'dsymm', 'dsyr2k', 'dsyrk', 'dtrmm', 'dtrsm'], 'list_routines, level 3, double: %s' % d3)
check(result(5).get('structuredContent', {}).get('blas_backend'), 'describe_machine: no BLAS backend')

ddot = result(6).get('structuredContent', {})
check(result(6).get('isError') is False and ddot.get('verified') is True, 'run_benchmark ddot: %s' % text(6))
check([r.get('dim1') for r in ddot.get('rows', [])] == [64, 128, 256], 'run_benchmark ddot: a row a size')
check(os.path.isfile(os.path.join(T, 'results', ddot.get('file', ''))), 'run_benchmark ddot: no result file')
gemm = result(7).get('structuredContent', {})
check(gemm.get('arguments', {}).get('trans') == 'NT', 'run_benchmark dgemm: not run with trans NT: %s' % text(7))
check(result(8).get('structuredContent', {}).get('rows') == gemm.get('rows'), 'read_result: not the run\'s rows')
refused(9, 'outside the results directory')
refused(10, 'is not what it takes')
refused(11, 'CPUs')
refused(12, 'no routine dfoo')
refused(13, 'no argument foo')
refused(14, 'refused the arguments')
suite = result(15).get('structuredContent', {})
check([r['routine'] for r in suite.get('results', [])] == ['ddot'] and suite.get('failed') is False, 'run_suite: %s' % text(15))
check(os.path.isfile(result(16).get('structuredContent', {}).get('page', '')), 'make_report: no page')
refused(17, 'already in the results directory')
check(by.get(18, {}).get('error', {}).get('code') == -32602, 'an unknown tool: not -32602')
check(by.get(19, {}).get('error', {}).get('code') == -32601, 'an unknown method: not -32601')
check(by.get(None, {}).get('error', {}).get('code') == -32700, 'a line that is not JSON: not -32700')
check(by.get(21, {}).get('result') == {}, 'ping')

for f, words in (('memory', 'MB of operands'), ('time', 'time cap'), ('wrong', 'Wrong result')):
    r = json.loads(open(os.path.join(T, f)).read())['result']
    t = ' '.join(c['text'] for c in r['content'])
    check(r.get('isError') is True and words in t, '%s: not refused with "%s": %s' % (f, words, t))

for b in bad:
    print('FAIL: ' + b)
sys.exit(1 if bad else 0)
EOF
echo "ok   the handshake, every tool, each refusal, the memory limit, the time cap, a wrong result, malformed messages"

rm -rf "$T"
echo "all checks passed"
