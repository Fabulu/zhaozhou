"""Rung sweep driver: ONE binary, N env rungs, a plate, and NO leftover frames.

Written 2026-09-09 for Owner Direction 10 (the white lightning line in a deep
dark blue), but nothing here is D10-specific -- it is the shape every "the
owner asked for options, so the deliverable is the AXIS" pass has hand-rolled.

It exists because of two CLAUDE.md laws that keep colliding:

* **A ladder must come from ONE BINARY** (10-GATE item 26), so the rungs differ
  only by environment and the binary md5 is recorded beside the plate.
* **A rule that HIDES waste is not a rule that removes it.** A 420-frame clip is
  ~116 MB of .rgb. Six rungs on two backdrops is 1.4 GB of intermediates that
  `.gitignore` makes invisible and nothing deletes. So this renders a rung,
  extracts ONLY the frames the plate needs, and purges the frame directory
  before the next rung -- peak disk is one clip, not the whole sweep.

    python rungsweep.py --exe ../../build-reel/bin/zhao-reel-cel.exe         --subject manafold-channel --frames 363,5 --out PLATEDIR         --crop 88,56,96,72 --scale 4         --rung "BASE:ZHAO_U02_STRAND=0"         --rung "C3 D8:ZHAO_U02_STRAND=1,ZHAO_U02_STRAND_CORE_R=3"

Every rung is `LABEL:K=V,K=V`. `--env` adds a variable to EVERY rung (that is
where the backdrop and ZIXX_EXP=celmain go), so the sweep cannot accidentally
change two things at once.

⚠ `--sep` EXISTS BECAUSE THIS TOOL SILENTLY COULD NOT ASK ONE QUESTION.
Rung settings split on `,`, and `U02_SHELL_TINT`'s value is itself `r,g,b`. So
`U02_SHELL_TINT=255,214,232` was shredded into a variable named `214` and a
variable named `232`, the reel's sscanf rejected the remaining "255", and the
rung rendered **at the default tint while claiming to be a tint rung** -- a
ladder whose rungs are all secretly identical, which is the most expensive shape
of wrong evidence there is.

Pass 15 shipped a fog shell whose findings called the tint "the axis that
decides fog vs bleach" and "never swept in any pass", and the by-eye review then
held the publish because the shell BLEACHED. **The axis nobody swept was the
axis this driver could not express**, and no `PROVENANCE.txt` in
`pass15-fx-plates/` carries a TINT rung. A missing tool feature and a missing
finding were the same fact (10-GATE item 42, one level down: a question your
instrument cannot phrase is a question nobody asks).

`--sep ";"` then gives `LABEL:U02_SHELL_TINT=235,70,170;U02_SHELL_ALPHA=520`.
The default stays "," so every ladder already recorded still replays exactly.

It never deletes anything it did not create: the frame directory it purges is
one it made itself under --out/.frames, and it refuses to run if that path
exists already.
"""
import argparse, os, shutil, subprocess, sys, hashlib

def md5(p):
    h = hashlib.md5()
    with open(p, "rb") as fh:
        for b in iter(lambda: fh.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", required=True)
    ap.add_argument("--subject", required=True)
    ap.add_argument("--frames", required=True, help="comma list, e.g. 363,5")
    ap.add_argument("--out", required=True)
    ap.add_argument("--crop", default=None, help="X,Y,W,H in native 384x240 px")
    ap.add_argument("--scale", type=int, default=4)
    ap.add_argument("--cols", type=int, default=0)
    ap.add_argument("--env", action="append", default=[], help="K=V for ALL rungs")
    ap.add_argument("--rung", action="append", required=True, help="LABEL:K=V,K=V")
    ap.add_argument("--sep", default=",",
                    help="separator BETWEEN rung settings (default ','). Use ';' "
                         "when a VALUE contains a comma, e.g. U02_SHELL_TINT=r,g,b")
    ap.add_argument("--keep", action="store_true", help="do NOT purge frames")
    a = ap.parse_args()

    exe = os.path.abspath(a.exe)
    if not os.path.isfile(exe):
        sys.exit("no such binary: " + exe)
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    scratch = os.path.join(out, ".frames")
    if os.path.exists(scratch):
        sys.exit("refusing to run: " + scratch + " exists; move it aside first")

    frames = [int(x) for x in a.frames.split(",") if x.strip() != ""]
    base = dict(kv.split("=", 1) for kv in a.env)

    stamp = md5(exe)
    print("rungsweep: binary md5", stamp)
    kept = {f: [] for f in frames}
    labels = []
    for spec in a.rung:
        label, _, kvs = spec.partition(":")
        env = dict(os.environ)
        env.update(base)
        for kv in [x for x in kvs.split(a.sep) if x.strip()]:
            k, _, v = kv.partition("=")
            # A setting with no "=" is almost always a value that got split off
            # its own key by the wrong separator -- the exact fault --sep exists
            # for. Refuse it loudly instead of exporting a variable named "214".
            if not k or "=" not in kv:
                sys.exit("rung %r: %r is not K=V -- if a VALUE contains a comma, "
                         "pass --sep ';'" % (label, kv))
            env[k] = v
        labels.append(label)
        os.makedirs(scratch, exist_ok=True)
        print("rungsweep: rung", label, "->", kvs)
        rc = subprocess.call([exe, scratch, a.subject], env=env)
        if rc != 0:
            shutil.rmtree(scratch, ignore_errors=True)
            sys.exit("rung " + label + " FAILED rc=" + str(rc))
        src = os.path.join(scratch, a.subject)
        safe = label.replace(" ", "_").replace("/", "-")
        for f in frames:
            fn = "%04d.rgb" % f
            s = os.path.join(src, fn)
            if not os.path.isfile(s):
                shutil.rmtree(scratch, ignore_errors=True)
                sys.exit("rung " + label + ": missing frame " + fn)
            d = os.path.join(out, safe + "-" + fn)
            shutil.copyfile(s, d)
            kept[f].append((label, d))
        if not a.keep:
            shutil.rmtree(scratch, ignore_errors=True)

    with open(os.path.join(out, "RUNG-PROVENANCE.txt"), "w", encoding="utf-8") as fh:
        fh.write("binary: " + exe + os.linesep)
        fh.write("md5:    " + stamp + os.linesep)
        fh.write("subject:" + a.subject + os.linesep)
        fh.write("common: " + repr(base) + os.linesep)
        fh.write("sep:    " + repr(a.sep) + os.linesep)
        for spec in a.rung:
            fh.write("rung:   " + spec + os.linesep)

    here = os.path.dirname(os.path.abspath(__file__))
    for f in frames:
        cols = a.cols or len(kept[f])
        png = os.path.join(out, "rungs-f%04d.png" % f)
        if a.crop:
            args = [sys.executable, os.path.join(here, "plates.py"), "crop", png,
                    str(a.scale), str(cols)]
            args += [lab + ":" + path + "@" + a.crop for lab, path in kept[f]]
        else:
            args = [sys.executable, os.path.join(here, "plates.py"), "grid", png,
                    str(a.scale)]
            args += [lab + ":" + path for lab, path in kept[f]]
        subprocess.check_call(args)
    print("rungsweep: done ->", out)

if __name__ == "__main__":
    main()
