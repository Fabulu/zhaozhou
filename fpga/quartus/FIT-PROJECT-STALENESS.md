# The fit project goes stale silently, and the fitter is the only thing that says so

Written 2026-08-31, after the first fit attempted since the render path was
wired into `zhao_shell_top`. It took **five** attempts to reach the fitter, and
four of those failures were the same kind of staleness in
`fpga/quartus/shell_fit/zhao_shell_fit.qsf`.

This file lives next to the project rather than in a run folder, because every
pass creates a new run and this will happen again the next time a block gains a
port.

---

## What goes stale, in the order it bites

### 1. The source list

`zhao_shell_top` instantiates `zhao_geom_bin_pipe` and `zhao_raster_fbwrite`.
The QSF listed neither, nor their closure, so the project could not have
elaborated at all.

**This one is already guarded.** `run_shell_fit.ps1` asserts the QSF's
`SYSTEMVERILOG_FILE` list against `set(ZHAO_SHELL_RTL ...)` in
`tests/CMakeLists.txt`, element by element **and in order**. That check caught
it immediately and refused to run.

> **Do not hand-order the list.** Generate it from CMake's, in CMake's order.
> A correct set in the wrong order fails the same check.

### 2. The virtual pins

Every top-level port must be virtualised or Quartus makes it a real pad. The
device has **145 user I/O**; the shell has ~3,000 bits of ports.

Nothing guards this. The only signal is the fitter, and it reports a NUMBER
rather than a name:

```
Error (169281): There are 742 IO input pads in the design,
                but only 315 IO input pad locations available
Error (179000): Design requires 156 user-specified I/O pins -- too many
                to fit in the 145 user I/O pin locations available
```

### 3. Ports declared two to a line — the one that cost three attempts

```systemverilog
input logic signed [22:0] render_kx0_i, render_ky0_i,
input logic signed [20:0] render_ax_i,  render_ay_i,
input logic signed [11:0] render_min_x_i, render_max_x_i,
```

A scan that takes the first identifier per line virtualises the x half and
leaves the y half a pad. Eight ports were missed this way:

```
3 x 23 (render_ky0/1/2_i) + 3 x 21 (render_ay/by/cy_i) + 2 x 12 (render_max_x/y_i)
= 156
```

which is exactly the number the fitter kept reporting.

### 4. Unpacked-array ports

```systemverilog
input logic [1:0] hps_state_i [0:2]
```

Quartus expands these to `hps_state_i[0]`, `[1]`, `[2]`. Assignments are written
per element. **This turned out NOT to be the cause of anything** — adding 37 of
them moved the pin count by zero — but they are correct to have and cost
nothing.

---

## How to diagnose it in one step instead of four

**Read the fitter's own table. Do not do arithmetic on the number.**

```powershell
cd fpga\quartus\shell_fit
& 'C:\intelFPGA_lite\17.0\quartus\bin64\quartus_map.exe' zhao_shell_fit
& 'C:\intelFPGA_lite\17.0\quartus\bin64\quartus_fit.exe' zhao_shell_fit
```

then section 12 of `output_files/zhao_shell_fit.fit.rpt`:

```
; I/O Assignment Warnings                            ;
; Pin Name           ; Reason                        ;
; render_max_x_i[0]  ; Incomplete set of assignments ;
; render_ky0_i[0]    ; Incomplete set of assignments ;
```

That names them. Three rounds of guessing were spent on a number that was
sitting in a table the whole time.

`output_files/` is gitignored, so running the flow in place leaves nothing to
clean up.

---

## The trap that wasted a whole attempt

`run_shell_fit.ps1` builds its snapshot from **`git archive HEAD`**, not the
working tree. An uncommitted QSF edit is invisible to it, and the run reports
the number it reported before — which reads exactly like "the fix did nothing".

That is deliberate and correct: a fit result is only worth anything if it names
the exact commit it measured. **Commit before running.**

---

## What to do when a block gains a top-level port

1. add the sources to `set(ZHAO_SHELL_RTL ...)` in `tests/CMakeLists.txt`;
2. regenerate the QSF's source list from it, in order;
3. rescan the ports **with a multi-name-aware scan** and add every missing
   `VIRTUAL_PIN`, including per element for unpacked arrays;
4. commit;
5. run the fit.

A generator for step 3 would remove this failure mode entirely, and is the
obvious next improvement to this tooling.

---

## The fit must be launched DETACHED, or the harness takes it with the wrapper

Added 2026-08-31, after the **sixth** fit in this project was killed the same
way. The earlier run log recorded five and noted that "the flow is running
locally so its reports persist on disk regardless of what happens to a wrapper
process" — which was half right and cost an hour to correct.

**The reports persist. The Quartus process does not.** When an agent harness
kills a backgrounded shell, `quartus_fit.exe` is inside that process tree and
dies with it, mid-fitter, forty minutes in. What survives on disk is whatever
stage last completed — so a fit killed during the fitter leaves a `map.rpt` and
no `fit.rpt`, and the numbers you wanted are simply not there.

**Launch it out of the tree:**

```powershell
$log = "C:\programmieren\zencrifice\zhaozhou\fit-round2.log"
Start-Process -FilePath "powershell.exe" -WindowStyle Hidden -PassThru `
  -ArgumentList "-NoProfile","-NonInteractive","-Command",
    "& { . '...\tools\env\zhao-env.ps1'; cd '...\zhaozhou';
         .\tools\quartus\run_shell_fit.ps1 -Processors 2 *> '$log' }"
```

Then poll the log rather than holding a pipe to it. Two details that bite:

* **The log is UTF-16LE.** `grep` reports "binary file matches" and finds
  nothing. Read it with `iconv -f UTF-16LE -t UTF-8` first.
* **`-PassThru` gives you the PID**, which is the only honest way to answer
  "is it still running" — `tasklist | grep quartus` also works and is what to
  check before concluding a fit died.

**The tell that this happened:** the wrapper reports killed/failed, and
`tasklist` shows **zero** Quartus processes. If Quartus is still there, the fit
is fine and only the watcher died — reattach by polling the output directory
instead of restarting, because restarting throws away however long it had run.
