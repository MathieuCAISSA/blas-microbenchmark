#!/bin/sh
# Checks the Claude Code plugin (#264), the repository as a plugin
# marketplace: .claude-plugin/marketplace.json and plugin.json are JSON
# with the fields Claude Code reads, the marketplace's one plugin is the
# repository's root under the plugin's own name, every path in them
# exists, its skills are there, and its MCP server is bmb_mcp. The
# version is test_release_files.sh's to check.
#
# .claude-plugin/ is not in the tarball, so this SKIPs there; it also
# SKIPs without python3, which reads the JSON.
set -e

S=${srcdir:-.}
test -d "$S/.claude-plugin" || { echo "SKIP: no .claude-plugin in $S"; exit 77; }
command -v python3 >/dev/null 2>&1 || { echo "SKIP: no python3 to read the manifests"; exit 77; }

python3 - "$S" <<'PY'
import json, os, re, sys
S = sys.argv[1]
bad = []
def check(ok, what):
    if not ok:
        bad.append(what)
def load(name):
    try:
        with open(os.path.join(S, '.claude-plugin', name)) as f:
            return json.load(f)
    except (OSError, ValueError) as e:
        sys.exit('FAIL: .claude-plugin/%s: %s' % (name, e))
market, plugin = load('marketplace.json'), load('plugin.json')
NAME = r'^[A-Za-z0-9][A-Za-z0-9._-]*$'

check(re.match(NAME, market.get('name', '')), 'marketplace.json: no valid name')
check((market.get('owner') or {}).get('name'), 'marketplace.json: no owner.name')
plugins = market.get('plugins') or []
check(len(plugins) == 1, 'marketplace.json: not one plugin')
for p in plugins:
    check(p.get('name') == plugin.get('name'), 'marketplace.json: the plugin is not named as plugin.json names it')
    src = p.get('source')
    check(isinstance(src, str) and '..' not in src and os.path.isfile(os.path.join(S, src, '.claude-plugin', 'plugin.json')),
          'marketplace.json: the source %r holds no .claude-plugin/plugin.json' % src)
check(re.match(r'^[a-z0-9]+(-[a-z0-9]+)*$', plugin.get('name', '')), 'plugin.json: the name is not kebab-case')
check(plugin.get('description'), 'plugin.json: no description')
servers = plugin.get('mcpServers') or {}
check(len(servers) == 1 and all(s.get('command') == 'bmb_mcp' for s in servers.values()),
      'plugin.json: its MCP server is not bmb_mcp')
skills = sorted(d for d in os.listdir(os.path.join(S, 'skills'))
                if os.path.isfile(os.path.join(S, 'skills', d, 'SKILL.md')))
check(len(skills) >= 3, 'the plugin\'s skills/ holds %d skills' % len(skills))
for b in bad:
    print('FAIL: ' + b)
if bad:
    sys.exit(1)
print('ok   the marketplace and its plugin: %s, bmb_mcp and the skills %s' % (plugin['name'], ', '.join(skills)))
PY
