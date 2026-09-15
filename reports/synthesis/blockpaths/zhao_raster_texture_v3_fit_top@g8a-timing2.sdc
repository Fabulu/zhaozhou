# Generated per block by tools/quartus/run_block_fit.ps1.
# The shell SDC is deliberately NOT used here: it names gpu_clk/vid_clk/
# audio_clk, and leaf blocks name their clock port `clk`.
create_clock -name clk        -period 10.000 [get_ports {clk}]
create_clock -name clk_gpu    -period 10.000 [get_ports {clk_gpu}]
create_clock -name gpu_clk    -period 10.000 [get_ports {gpu_clk}]
create_clock -name vid_clk    -period 20.000 [get_ports {vid_clk}]
create_clock -name clk_audio  -period 40.000 [get_ports {clk_audio}]
create_clock -name audio_clk  -period 40.000 [get_ports {audio_clk}]
create_clock -name wr_clk     -period 10.000 [get_ports {wr_clk}]
create_clock -name rd_clk     -period 10.000 [get_ports {rd_clk}]
derive_clock_uncertainty

# ---- I/O DELAYS. Without these the Fmax below is not the block. ----
# Added 2026-08-23 (RUN-20260823-1736), and it is the SECOND HALF of the
# fix above rather than a refinement of it. Constraining the clock makes
# REGISTER-TO-REGISTER paths timed. It does nothing for pin-to-register or
# register-to-pin paths, which TimeQuest simply excludes when no I/O delay
# is declared -- and for a leaf block whose arithmetic sits BETWEEN its
# ports, that is most of the block.
#
# MEASURED, on three kept workspaces, by re-running quartus_sta on the
# databases the fits had already produced:
#
#   zhao_texture_tmu @pre-rearch   worst path: texture_samples_o[19]
#                                  -> texture_samples_o[27], 4.818 ns
#   zhao_texture_tmu FILT_LANES=4  worst path: texture_samples_o[3]
#                                  -> texture_samples_o[21], 4.634 ns
#
# Both blocks reported ~195 MHz. Both numbers are the 32-bit SATURATING
# SAMPLE COUNTER carry chain. The 32 multiplies, the format decode, the
# 48-bit address generator and the wrap folds appeared in NO timed path at
# all, because each of them runs from an input pin or to an output pin.
# The third workspace (FILT_LANES=2) differs only in that one filter output
# happens to land in a real register -- and there the worst path is 20.462
# ns, i.e. 48.9 MHz, through the very arithmetic the other two called fast.
#
# The model below is `same clock, no external budget`: every non-clock port
# is assumed driven by, or captured into, a register in a neighbouring block
# on this clock, with none of the period spent outside. That is optimistic
# about inter-block routing and exact about the logic inside the block,
# which is what a per-block characterisation is for.
#
# Guarded, because 8 of this design`s 71 clock ports are NOT called clk and
# an unguarded set_input_delay against a clock that does not exist is an
# error rather than a warning.
set _zhao_clk [get_ports -nowarn {clk}]
if {[get_collection_size $_zhao_clk] > 0} {
    set _zhao_clkports [get_ports -nowarn {clk clk_gpu gpu_clk vid_clk clk_audio audio_clk wr_clk rd_clk}]
    set _zhao_datain [remove_from_collection [all_inputs] $_zhao_clkports]
    if {[get_collection_size $_zhao_datain] > 0} {
        set_input_delay -clock clk 0.000 $_zhao_datain
    }
    if {[get_collection_size [all_outputs]] > 0} {
        set_output_delay -clock clk 0.000 [all_outputs]
    }
}
