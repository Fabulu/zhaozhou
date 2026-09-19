#!/usr/bin/env python3
"""Manafold version 18 Wave E: the NO-LIVE-HISTORY gate (Owner Direction 19 §8).

"Hasty has the old smear effect again, we're getting rid of that everywhere."

Two independent checks, each with its own positive control:

  LIST     kU02LiveSiteSubjects in zhao_reel.cpp must equal the live
           `renders/manafold-*.webm` entries of Upheaval/website/creatures.json
           (archive entries excluded). A subject added to the site but not to
           the renderer's table would escape the renderer assertion; this is
           what stops that.
  HISTORY  Render every live subject with the PRODUCTION invocation
           (ZIXX_EXP=celmain, ZIXX_LIGHT=diagonal-cool-cross). The renderer
           prints one `live-history:` line per live subject comparing the
           declared mist/smear flags against EXECUTED-block receipts, and
           returns RC 5 on any history. Normal requires RC 0 and 22 OK lines.

Controls (each must fire in its own category, and only there):
  --control legacy      ZHAO_U02_LIVE_MIST=legacy restores the version-17
                        builder default. Requires RC 5 and 22/22 FAIL lines
                        with executed mist frames > 0. LIST must stay green.
  --control list-drift  drops one name from the parsed renderer table before
                        comparing. Requires LIST to fail; HISTORY is not run.

Exit: 0 when the requested mode's expectation holds (normal: all green;
control: its detector fired), 1 otherwise, 2 on usage error.

Usage:
  python tools/reel/manafold_live_history_gate.py --renderer <zhao-reel-cel.exe>
      --out <scratch dir> [--site <creatures.json>] [--control legacy|list-drift]
"""
import argparse
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REEL_SRC = os.path.join(HERE, "zhao_reel.cpp")
DEFAULT_SITE = os.path.normpath(
    os.path.join(HERE, "..", "..", "..", "Upheaval", "website", "creatures.json"))
EXPECTED_LIVE = 22
LINE_RE = re.compile(
    r"^live-history: (\S+) declared mist=(\d) smear=(\d+) executed mist=(\d+) "
    r"smear=(\d+) frames.* -- (OK|FAIL)$")


def renderer_table():
    src = open(REEL_SRC, encoding="utf-8").read()
    m = re.search(r"kU02LiveSiteSubjects\[\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        raise SystemExit("gate: kU02LiveSiteSubjects not found in zhao_reel.cpp")
    return re.findall(r'"([^"]+)"', m.group(1))


def site_table(path):
    d = json.load(open(path, encoding="utf-8"))
    out = []

    def walk(o):
        if isinstance(o, dict):
            for v in o.values():
                walk(v)
        elif isinstance(o, list):
            for v in o:
                walk(v)
        elif isinstance(o, str):
            m = re.fullmatch(r"renders/(manafold-[a-z0-9-]+)\.webm", o)
            if m:
                out.append(m.group(1))

    walk(d)
    return out


def check_list(site_path, drop_one):
    reel = renderer_table()
    if drop_one:
        reel = reel[1:]
    site = site_table(site_path)
    ok = True
    if len(set(site)) != len(site):
        print("LIST FAIL: duplicate live site entries")
        ok = False
    missing = sorted(set(site) - set(reel))
    extra = sorted(set(reel) - set(site))
    if missing or extra:
        print(f"LIST FAIL: site-not-in-renderer {missing} renderer-not-on-site {extra}")
        ok = False
    if len(site) != EXPECTED_LIVE:
        print(f"LIST FAIL: site lists {len(site)} live Manafold subjects, expected {EXPECTED_LIVE}")
        ok = False
    if ok:
        print(f"LIST OK: renderer table == site live bank ({len(site)} subjects)")
    return ok, sorted(set(site))


def check_history(renderer, out, names, legacy):
    env = dict(os.environ)
    env["ZIXX_EXP"] = "celmain"
    env["ZIXX_LIGHT"] = "diagonal-cool-cross"
    env.pop("ZHAO_U02_LIVE_MIST", None)
    if legacy:
        env["ZHAO_U02_LIVE_MIST"] = "legacy"
    os.makedirs(out, exist_ok=True)
    p = subprocess.run([os.path.abspath(renderer), os.path.abspath(out)] + names, env=env, capture_output=True, text=True)
    rows = {}
    for ln in p.stdout.splitlines():
        m = LINE_RE.match(ln.strip())
        if m:
            rows[m.group(1)] = (int(m.group(2)), int(m.group(3)), int(m.group(4)),
                                int(m.group(5)), m.group(6))
    missing = [n for n in names if n not in rows]
    n_ok = sum(1 for r in rows.values() if r[4] == "OK")
    n_fail = sum(1 for r in rows.values() if r[4] == "FAIL")
    fired = sum(1 for r in rows.values() if r[4] == "FAIL" and r[2] > 0)
    print(f"HISTORY renderer RC {p.returncode}: {len(rows)}/{len(names)} lines, "
          f"OK {n_ok}, FAIL {n_fail} (executed-mist FAIL {fired}), missing {missing}")
    for n in names:
        if n in rows:
            r = rows[n]
            print(f"  {n}: declared mist={r[0]} smear={r[1]} executed mist={r[2]} "
                  f"smear={r[3]} {r[4]}")
    if legacy:
        return p.returncode == 5 and not missing and fired == len(names)
    return p.returncode == 0 and not missing and n_ok == len(names)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--renderer", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--site", default=DEFAULT_SITE)
    ap.add_argument("--control", choices=["legacy", "list-drift"])
    a = ap.parse_args()
    if not os.path.isfile(a.renderer) or not os.path.isfile(a.site):
        print("gate: renderer or site JSON not found", file=sys.stderr)
        return 2

    if a.control == "list-drift":
        ok, _ = check_list(a.site, drop_one=True)
        print("CONTROL list-drift: LIST detector " + ("did NOT fire -- FAIL" if ok else "fired -- OK"))
        return 0 if not ok else 1

    list_ok, names = check_list(a.site, drop_one=False)
    if not list_ok:
        print("CONTROL legacy: LIST failed outside its own control" if a.control else "FAIL")
        return 1
    hist_ok = check_history(a.renderer, a.out, names, legacy=a.control == "legacy")
    if a.control == "legacy":
        print("CONTROL legacy: HISTORY detector " +
              ("fired on every live subject -- OK" if hist_ok else "did NOT fire everywhere -- FAIL"))
    else:
        print("PASS: no persistent history on any live Manafold subject" if hist_ok else "FAIL")
    return 0 if hist_ok else 1


if __name__ == "__main__":
    sys.exit(main())
