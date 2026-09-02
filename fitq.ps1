$ErrorActionPreference='Continue'
# One process, one fit at a time. `powershell -File` FLATTENS an array parameter
# into positional arguments -- the first attempt died instantly on every row with
# "Es wurde kein Positionsparameter gefunden", which in the report would have
# read as nine blocks that do not fit. Call the script in-process instead.
$src = @(
 'fpga/rtl/texture/zhao_texture_cache_pipe.sv',
 'fpga/rtl/texture/zhao_texture_aux_pipe.sv',
 'fpga/rtl/texture/zhao_texture_aux_div6.sv',
 'fpga/rtl/texture/zhao_texture_palette_res.sv',
 'fpga/rtl/texture/zhao_texture_rsp_dispatch.sv',
 'fpga/rtl/texture/zhao_texture_bilerp_lane.sv',
 'fpga/rtl/texture/zhao_texture_mosaic.sv',
 'fpga/rtl/texture/zhao_texture_mod255.sv',
 'fpga/rtl/raster/zhao_raster_rcp24_svc.sv',
 'fpga/rtl/raster/zhao_raster_perspuv_svc.sv',
 'fpga/rtl/raster/zhao_raster_texjoin_v2.sv',
 'fpga/rtl/field/zhao_field_rcp24_rom.sv'
)
$mods = @(
 'zhao_texture_cache_pipe',
 'zhao_raster_perspuv_svc',
 'zhao_raster_texjoin_v2',
 'zhao_raster_rcp24_svc',
 'zhao_texture_aux_pipe',
 'zhao_texture_palette_res',
 'zhao_texture_rsp_dispatch',
 'zhao_texture_bilerp_lane',
 'zhao_texture_mosaic'
)
foreach ($m in $mods) {
  Write-Host "=== FIT $m ==="
  try { & .\tools\quartus\run_block_fit.ps1 -Module $m -ExtraSources $src -TimeoutSeconds 9000 }
  catch { Write-Host "ROW FAILED $m : $_" }
  Write-Host "=== DONE $m ==="
}
Write-Host "=== QUEUE DONE ==="
