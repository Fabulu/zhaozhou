# fire-tests.ps1 -- prove every CHECK CAN FAIL.
#
# reports/TEXTURE-ISLAND-V3-ARCHITECTURE-20260906.txt Appendix E, "evidence
# required per change": "real rebuilt test result; NEGATIVE MUTATION THAT
# FIRES". And section 23.10 asks for mutation tests of the testbench itself.
# A green suite proves nothing until each of its mechanisms has been shown to
# go red for the right reason.
#
# Each entry perturbs ONE mechanism in the RTL by an exact literal string
# replacement, rebuilds, runs, records the failure text, and restores the file
# byte-for-byte. The restore is verified by hash, not by trust.

$ErrorActionPreference = 'Stop'
$repo  = 'C:\programmieren\zencrifice\zhaozhou'
$bdir  = Join-Path $repo 'subagents\20260906-1858-v3-completion-bank-experiment\build'
$own   = Join-Path $repo 'fpga\rtl\texture\zhao_texture_v3own.sv'
$outmd = Join-Path $repo 'subagents\20260906-1858-v3-completion-bank-experiment\FIRE-TEST-EVIDENCE.md'

. (Join-Path $repo 'tools\env\zhao-env.ps1')
$env:PATH = 'C:\programmieren\dsstuff\mingw64\bin;' + $env:PATH

# QUARTUS_GOTCHAS 16's cousin, and it killed the first run of this script.
# PowerShell 5.1 wraps a NATIVE executable's stderr lines in ErrorRecords when
# they are redirected, and with $ErrorActionPreference = 'Stop' that becomes a
# TERMINATING error even though the exe exited 0. Verilator prints a harmless
# STDOUT_FILENO-redefined warning from mingw headers on every build, so the
# script died on its own baseline. stderr is redirected to a FILE here instead
# of into the pipeline, and the preference is dropped to Continue around the
# native calls. The exit code is read from $LASTEXITCODE directly.
function Rebuild-And-Run {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $blog = Join-Path $bdir '_firetest_build.log'
    $tlog = Join-Path $bdir '_firetest_run.log'
    Push-Location $repo
    & verilator_bin.exe --cc --exe --build --build-jobs 6 --assert -Wall `
        --top-module zhao_texture_v3own --prefix Vzhao_texture_v3own --Mdir $bdir `
        -CFLAGS "-std=c++17 -O1" -o test_texture_v3own_adversarial `
        fpga/rtl/texture/zhao_texture_v3bank.sv `
        fpga/rtl/texture/zhao_texture_v3rq.sv `
        fpga/rtl/texture/zhao_texture_v3own.sv `
        tests/texture/texture_v3own_adversarial.cpp `
        tests/harness/zhao_sim.cpp > $blog 2> ($blog + '.err')
    $brc = $LASTEXITCODE
    Pop-Location
    if ($brc -ne 0) {
        $ErrorActionPreference = $prev
        return @{ build = $brc; rc = $null; out = @('BUILD FAILED rc=' + $brc) }
    }
    Push-Location $bdir
    & .\test_texture_v3own_adversarial.exe > $tlog 2> ($tlog + '.err')
    $rc = $LASTEXITCODE
    Pop-Location
    $ErrorActionPreference = $prev
    $o = @()
    if (Test-Path $tlog) { $o += Get-Content $tlog }
    if (Test-Path ($tlog + '.err')) { $o += Get-Content ($tlog + '.err') }
    return @{ build = 0; rc = $rc; out = $o }
}

# name ; find ; replace ; what the mutation breaks
$mutations = @(
  @{ n = 'M1 recent-claim forwarding removed'
     f = "                      && ((c1t_cmt_q & c1t_bit_c) == 4'd0)`n                      && !fwd_t_hit_c;"
     r = "                      && ((c1t_cmt_q & c1t_bit_c) == 4'd0);"
     w = 'D.1 consecutive duplicate: the second packet must be accepted' },

  @{ n = 'M2 sample bank write enable made unconditional'
     f = "      c3t_we_q <= c2t_acc_c ? c1t_bit_c[2:0] : 3'b000;"
     r = "      c3t_we_q <= c1t_bit_c[2:0];"
     w = 'a rejected packet must reach the payload bank and overwrite it' },

  @{ n = 'M3 owner freed at the FINAL WRITE instead of at output (D.6)'
     f = "      if (out_fire_c && (emit_q == SLOTW'(i))) live_n_c[i] = 1'b0;"
     r = "      if (out_fire_c && (emit_q == SLOTW'(i))) live_n_c[i] = 1'b0;`n      if (c4f_v_q && (c4f_slot_q == SLOTW'(i))) live_n_c[i] = 1'b0;"
     w = 'the ring must wrap over a live owner and corrupt its context' },

  @{ n = 'M4 ready-ticket coalescing removed (D.2)'
     f = "  assign tkt_a_c  = a_elig_c && !same_owner_c;"
     r = "  assign tkt_a_c  = a_elig_c;"
     w = 'one owner must receive TWO ready tickets' },

  @{ n = 'M5 lost update: stale counter increments once for two faults'
     f = "    d_stale_c = 2'd0;`n    if (c2t_stale_c) d_stale_c = d_stale_c + 2'd1;`n    if (c2a_stale_c) d_stale_c = d_stale_c + 2'd1;"
     r = "    d_stale_c = 2'd0;`n    if (c2t_stale_c) d_stale_c = 2'd1;`n    if (c2a_stale_c) d_stale_c = 2'd1;"
     w = 'simultaneous TMU and AUX faults must count ONE instead of TWO' },

  @{ n = 'M6 ordered retirement allowed to skip the incomplete head'
     f = "  assign fetch_fire_c = (unf_cnt_q != '0)`n                     && live_q[fetch_q] && fdn_q[fetch_q] && !ftc_q[fetch_q]"
     r = "  assign fetch_fire_c = (unf_cnt_q != '0)`n                     && live_q[fetch_q] && !ftc_q[fetch_q]"
     w = 'the output must emit an owner whose final result was never written' },

  @{ n = 'M7 sample_index range check removed'
     f = "  assign c1t_bit_c = c1t_rng_q ? (4'b0001 << c1t_sidx_q) : 4'b0000;"
     r = "  assign c1t_bit_c = (4'b0001 << c1t_sidx_q);"
     w = 'sample_index 3 must stop being counted as a range fault' },

  @{ n = 'M8 generation check removed from the C2 predicate'
     f = "  assign c2t_idok_c    = c1t_live_q && (c1t_tgen_q == c1t_gen_q);"
     r = "  assign c2t_idok_c    = c1t_live_q;"
     w = 'a stale-generation packet must be accepted as a real completion' },

  @{ n = 'M9 generation-wrap drain gate removed (section 5.5)'
     f = "  assign adm_ready_o  = (live_cnt_q < CNTW'(OWNERS)) && (!wrap_block_c || quiet_c);"
     r = "  assign adm_ready_o  = (live_cnt_q < CNTW'(OWNERS));"
     w = 'the namespace must wrap without draining the island' },

  @{ n = 'M10 output reservation off-by-one (19.4 lost beat / rate loss)'
     f = "                     && ((out_res_q - CNTW'(out_fire_c)) < CNTW'(OUTQD));"
     r = "                     && (out_res_q < CNTW'(OUTQD));"
     w = 'the retirement path must stop sustaining one fragment per clock' },

  @{ n = 'M11 committed published in the same stage as the write (data-before-done)'
     f = "      c4t_v_q  <= c3t_v_q;"
     r = "      c4t_v_q  <= c2t_acc_c;"
     w = 'committed must rise before the payload write edge' },

  # M13 EXISTS BECAUSE M7 DID NOT FIRE, and that non-fire is evidence rather
  # than a failed attempt. M7 removed the range ternary from `c1t_bit_c` and
  # the suite still passed at 460/460 -- because out-of-range returns are
  # refused TWICE independently: `c1t_rng_q` gates the acceptance predicate on
  # its own, AND the source-bit mask is forced to zero so an index-3 packet
  # cannot address a texture bank at all. Deleting one defence leaves the other
  # standing, exactly as `zhao_texture_fragrob.sv:436` says it intends ("being
  # refused by the predicate and being unable to address anything are two
  # independent defences, on purpose").
  #
  # So M7 mutated a REDUNDANT check while the test observes the live one, and
  # a green run there proves nothing about case 7. M13 removes the range test
  # at its source instead, which takes both defences out with one edit.
  @{ n = 'M13 range check removed AT SOURCE (both defences at once)'
     f = "    c0t_rng_q  <= (tmu_rhandle_i[GENW+1 -: 2] != 2'd3);"
     r = "    c0t_rng_q  <= 1'b1;"
     w = 'sample_index 3 must stop being a range fault AND must reach a source bit' },

  # M12 v2. The FIRST version deleted `!c1f_fcl_q` and `!fwd_f_hit_c` from the
  # predicate, which left both signals with no consumer -- and the simulator's
  # -Wall UNUSEDSIGNAL made the BUILD fail, so the run recorded an empty exit
  # code (QUARTUS_GOTCHAS 16's signature) instead of a fire. A mutation that
  # does not compile proves nothing.
  #
  # This version stops the claim from ever being SET instead, which keeps every
  # signal read and is the minimal expression of "final_claimed is gone".
  @{ n = 'M12 final_claimed never set -- duplicate final may overwrite (18.4)'
     f = "      if (c2f_acc_c && (c1f_slot_q == SLOTW'(i)))`n        fcl_n_c[i] = 1'b1;"
     r = "      if (c2f_acc_c && (c1f_slot_q == SLOTW'(i)))`n        fcl_n_c[i] = 1'b0;"
     w = 'the +2 duplicate FINAL must be accepted and overwrite the result' }
)

$orig = [System.IO.File]::ReadAllText($own)
$origHash = (Get-FileHash -Algorithm SHA256 $own).Hash

$lines = @()
$lines += '# Fire-test evidence -- every check proved able to fail'
$lines += ''
$lines += ('Generated ' + (Get-Date -Format 'yyyy-MM-dd HH:mm') + ' from ' +
           'fpga/rtl/texture/zhao_texture_v3own.sv')
$lines += ('Baseline SHA256 of the module under test: ' + $origHash)
$lines += ''
$lines += 'Each row perturbs ONE mechanism, rebuilds from source, runs the'
$lines += 'adversarial bench, and records the VERBATIM failure text. The file is'
$lines += 'then restored and the restore is verified by hash.'
$lines += ''

# baseline
$b = Rebuild-And-Run
$lines += '## BASELINE (unperturbed)'
$lines += ''
$lines += '```'
$lines += ($b.out | Select-Object -Last 6)
$lines += ("exit code: " + $b.rc)
$lines += '```'
$lines += ''
Write-Host ("BASELINE rc=" + $b.rc)

# V3_FIRE_ONLY = a comma-separated list of mutation ids ("M1,M5") to run just
# those. Pass 1 of this script was written with CRLF in four multi-line anchors
# while the RTL file is LF-only, so those four reported "anchor not found" --
# which the script correctly recorded as a HOLE IN THE EVIDENCE rather than
# passing over silently. This lets the second pass cover exactly them instead
# of re-running the eight that already produced evidence.
$only = @()
if ($env:V3_FIRE_ONLY) { $only = $env:V3_FIRE_ONLY -split ',' }
if ($env:V3_FIRE_OUT)  { $outmd = $env:V3_FIRE_OUT }

foreach ($m in $mutations) {
    $id = $m.n.Split(' ')[0]
    if ($only.Count -gt 0 -and ($only -notcontains $id)) { continue }
    Write-Host ("=== " + $m.n)
    $txt = [System.IO.File]::ReadAllText($own)
    $idx = $txt.IndexOf($m.f)
    if ($idx -lt 0) {
        $lines += ('## ' + $m.n)
        $lines += ''
        $lines += 'MUTATION COULD NOT BE APPLIED -- the anchor string was not found.'
        $lines += 'That is itself a finding: the evidence below does not cover this mechanism.'
        $lines += ''
        Write-Host '  !! anchor not found'
        continue
    }
    $last = $txt.LastIndexOf($m.f)
    $uniq = ($idx -eq $last)
    $new = $txt.Substring(0, $idx) + $m.r + $txt.Substring($idx + $m.f.Length)
    [System.IO.File]::WriteAllText($own, $new)

    $res = Rebuild-And-Run
    $fails = @($res.out | Where-Object { $_ -match '^FAIL:' })
    $verdict = @($res.out | Where-Object { $_ -match 'checks (passed|FAILED)' })

    $lines += ('## ' + $m.n)
    $lines += ''
    $lines += ('Anchor unique in file: ' + $uniq)
    $lines += ('Expected to break: ' + $m.w)
    $lines += ''
    $lines += ('Exit code: ' + $res.rc + '   (0 = the mutation did NOT fire, which would be a hole)')
    $lines += ('Failing checks: ' + $fails.Count)
    $lines += ''
    $lines += '```'
    if ($fails.Count -eq 0) {
        # NO `FAIL:` LINES IS NOT NO EVIDENCE. A firing SystemVerilog assertion
        # ends the run through $stop/abort, so the suite never reaches its
        # verdict line and never prints a FAIL. Recording only "(no FAIL lines)"
        # threw away the most precise evidence the harness produces -- the
        # assertion NAME and its LINE NUMBER. Dump the raw tail instead.
        $lines += '(no FAIL: lines -- the run aborted; raw output follows)'
        $lines += ($res.out | Select-Object -Last 14)
    } else {
        $lines += ($fails | Select-Object -First 12)
        if ($fails.Count -gt 12) { $lines += ('... and ' + ($fails.Count - 12) + ' more') }
    }
    $lines += $verdict
    $lines += '```'
    $lines += ''
    Write-Host ("  rc=" + $res.rc + " fails=" + $fails.Count)

    [System.IO.File]::WriteAllText($own, $orig)
    $h = (Get-FileHash -Algorithm SHA256 $own).Hash
    if ($h -ne $origHash) { throw ("RESTORE FAILED after " + $m.n) }
}

# final restore proof
$b2 = Rebuild-And-Run
$lines += '## RESTORED (proof the tree is back to baseline)'
$lines += ''
$lines += '```'
$lines += ('SHA256 after restore: ' + (Get-FileHash -Algorithm SHA256 $own).Hash)
$lines += ($b2.out | Select-Object -Last 4)
$lines += ("exit code: " + $b2.rc)
$lines += '```'

[System.IO.File]::WriteAllLines($outmd, $lines)
Write-Host ("WROTE " + $outmd)
Write-Host ("RESTORED rc=" + $b2.rc)
