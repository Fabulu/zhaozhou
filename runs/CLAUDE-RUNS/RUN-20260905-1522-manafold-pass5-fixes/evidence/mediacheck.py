"""Every media file declared in index.html must exist (gate checklist 19).
Scans src=, poster=, href= for renders/ paths -- the broad scan QA used
(897 refs), not the reviewer's narrower 453."""
import os, re, sys
site = sys.argv[1]
html = open(os.path.join(site, "public", "index.html"), encoding="utf-8").read()
refs = re.findall(r'(?:src|poster|href)="(renders/[^"]+)"', html)
missing = [r for r in sorted(set(refs)) if not os.path.exists(os.path.join(site, "public", r))]
print(f"declared render refs: {len(refs)} ({len(set(refs))} unique); missing: {len(missing)}")
for m in missing:
    print("  MISSING:", m)
noindex = html.count('name="robots"')
print(f'robots meta tags: {noindex}')
sys.exit(1 if missing or noindex != 1 else 0)
