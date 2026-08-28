# Detached fit runner. Launched with Start-Process so it is NOT a child of the
# agent task -- four fit attempts have been lost to the task being killed
# while Quartus was mid-flight, and a place-and-route here runs 1-4 hours.
Set-Location C:\programmieren\zencrifice\zhaozhou
$log = 'runs\CLAUDE-RUNS\RUN-20260827-1747-field-v3-rearchitecture\fit_probes45_retry.log'
"=== detached runner, probe 5 then probe 4, 14000s each ===" | Out-File -Encoding utf8 $log
"started $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" | Out-File -Append -Encoding utf8 $log
foreach ($m in @(
    @{n='zhao_probe_patch_acc';  s='fpga/rtl/synth/zhao_probe_patch_acc.sv'},
    @{n='zhao_probe_curve_svc';  s='fpga/rtl/synth/zhao_probe_curve_svc.sv'})) {
  "--- $($m.n) begin $(Get-Date -Format 'HH:mm:ss') ---" | Out-File -Append -Encoding utf8 $log
  & powershell -NoProfile -ExecutionPolicy Bypass -File tools\quartus\run_block_fit.ps1 `
      -Module $m.n -ExtraSources $m.s -TimeoutSeconds 14000 *>&1 |
    Out-File -Append -Encoding utf8 $log
  "--- $($m.n) end $(Get-Date -Format 'HH:mm:ss') ---" | Out-File -Append -Encoding utf8 $log
}
"ALL DONE $(Get-Date -Format 'HH:mm:ss')" | Out-File -Append -Encoding utf8 $log
