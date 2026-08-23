[CmdletBinding()]
param(
    [string]$Only = '',
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Continue'

# ---------------------------------------------------------------------------
# The map-only sweep over the whole repository, RUN-20260823-2226.
#
# ONE QUARTUS JOB AT A TIME. This script is deliberately a serial foreach and
# not a throttled ForEach-Object -Parallel. Concurrent fits exhausted the 24 GB
# machine (QUARTUS_GOTCHAS 7) and permanently lost zhao_geom_project's fit row,
# which is one of the two reasons this whole audit exists.
#
# It invokes run_block_map.ps1 ONCE PER MODULE rather than passing the whole
# list, because that script writes reports/synthesis/zhao_block_map.json at the
# END of its module list. A ninety-module invocation that dies at module eighty
# writes nothing at all. One invocation per module means the JSON is complete
# and committed-shaped after every single module, at the cost of a few seconds
# of re-merge each time.
#
# ORDER IS BY AUDIT VALUE, not alphabetically. The predictions the owner asked
# to confirm or refute come first, so a sweep that has to be stopped early has
# still answered the questions that were asked.
# ---------------------------------------------------------------------------

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$runner = Join-Path $PSScriptRoot 'run_block_map.ps1'

# Tier 1 -- the six predictions and the two heatmap calibration blocks.
$tier1 = @(
    'zhao_geom_project',      # P1: duplicate projector
    'zhao_geom_quat2mat',     # P2: nine quaternion products at once
    'zhao_geom_mat3x4_mul',   # P2: three 32x32 + saturation per cycle
    'zhao_geom_pose_decode',  # P2/P6
    'zhao_geom_pose_cache',   # P6: the pose palette storage
    'zhao_forge_cliff',       # P3: three async reads over ~120 kbit
    'zhao_raster_fragment',   # P4: mutually exclusive blend products
    'zhao_terrain_normals',   # P5: six 33x33 products
    'zhao_field_seq',         # heatmap calibration: must light RED
    'zhao_texture_tmu'        # heatmap calibration: must light RED
)

# Tier 2 -- every other module with real arithmetic or real storage.
$tier2 = @(
    'zhao_terrain_project', 'zhao_terrain_tess', 'zhao_terrain_patch',
    'zhao_terrain_lod', 'zhao_terrain_bake', 'zhao_terrain_bake_delta',
    'zhao_terrain_velocity',
    'zhao_geom_skin', 'zhao_geom_cull', 'zhao_geom_clip', 'zhao_geom_lod',
    'zhao_geom_setup', 'zhao_geom_binner', 'zhao_geom_bin_pipe', 'zhao_geom_arena',
    'zhao_raster_edgewalk', 'zhao_raster_blend', 'zhao_raster_earlyz',
    'zhao_raster_fill', 'zhao_raster_quant', 'zhao_raster_resolve',
    'zhao_raster_tilestore', 'zhao_raster_tile_pipe', 'zhao_raster_div255',
    'zhao_surface_stamp', 'zhao_surface_sheet', 'zhao_surface_blend', 'zhao_surface_sq',
    'zhao_texture_cache', 'zhao_texture_bilerp', 'zhao_texture_aux',
    'zhao_texture_mosaic', 'zhao_texture_mod255',
    'zhao_field_alu', 'zhao_field_exec_shared', 'zhao_field_isqrt',
    'zhao_field_normalize', 'zhao_field_mul', 'zhao_field_len', 'zhao_field_rot',
    'zhao_field_curve', 'zhao_field_noise', 'zhao_field_sin', 'zhao_field_sin_rom',
    'zhao_field_rcp', 'zhao_field_rcp_rom', 'zhao_field_rcp24_rom',
    'zhao_field_ring', 'zhao_field_progcache',
    'zhao_part_expand', 'zhao_part_soft'
)

# Tier 3 -- shell-cone and infrastructure modules. Most already have a FIT row;
# a map row still buys the RAM-inference column the fit summary does not carry.
$tier3 = @(
    'zhao_cmd_dma', 'zhao_cmd_decoder', 'zhao_cmd_scheduler',
    'zhao_video_scanout', 'zhao_video_framectl', 'zhao_video_mode',
    'zhao_video_scaler', 'zhao_video_slotmgr',
    'zhao_scanout_fetch', 'zhao_scanout_linebuf', 'zhao_scanout_serializer',
    'zhao_sdram_ctrl', 'zhao_vram_arbiter', 'zhao_mem_guard',
    'zhao_hps_bridge', 'zhao_hps_arbiter',
    'zhao_audio_fifo', 'zhao_input_snapshot', 'zhao_input_rumble',
    'zhao_debug_counters', 'zhao_debug_crc', 'zhao_debug_frameblit', 'zhao_debug_trace',
    'zhao_measure_governor', 'zhao_measure_tokens',
    'zhao_crc32c_fold', 'zhao_dc_sdp_ram', 'zhao_synth_probe', 'zhao_stub_top'
)

# EXCLUDED, each with its reason recorded rather than silently omitted:
#   zhao_shell_top -- the composed top. Measured 2026-08-18: elaboration alone
#                     exceeded ten minutes and 16 GB in Quartus 17.0.2 Lite. It
#                     has its own lane (run_composed_fit.ps1) and belongs there.
$excluded = @{ 'zhao_shell_top' = 'composed top; elaboration exceeds 10 min / 16 GB in 17.0.2 Lite. Use run_composed_fit.ps1.' }

$order = @()
$order += $tier1
$order += $tier2
$order += $tier3

if ($Only) { $order = @($order | Where-Object { $_ -like $Only }) }

Write-Host ("map sweep: {0} module(s), serial, one Quartus job at a time" -f $order.Count)
foreach ($k in $excluded.Keys) { Write-Host ("  EXCLUDED {0}: {1}" -f $k, $excluded[$k]) }
if ($DryRun) { $order | ForEach-Object { Write-Host "  $_" }; return }

$i = 0
foreach ($m in $order) {
    $i++
    $stamp = (Get-Date).ToString('HH:mm:ss')
    Write-Host ("[{0}/{1}] {2} {3}" -f $i, $order.Count, $stamp, $m)
    try {
        & $runner -Module $m
    } catch {
        Write-Warning ("{0}: driver threw: {1}" -f $m, $_.Exception.Message)
    }
}
Write-Host 'map sweep complete'
