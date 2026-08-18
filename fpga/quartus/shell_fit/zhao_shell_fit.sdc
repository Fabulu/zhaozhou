# PROVISIONAL shell characterization clocks only. These periods mirror the
# committed 100 MHz cost-model assumption and the frozen simulation ratios
# vid_clk=gpu_clk/2, audio_clk=gpu_clk/4. They are not board clock truth.
create_clock -name gpu_clk   -period 10.000 [get_ports {gpu_clk}]
create_clock -name vid_clk   -period 20.000 [get_ports {vid_clk}]
create_clock -name audio_clk -period 40.000 [get_ports {audio_clk}]

# AUDIO.FIFO is explicitly dual-clock. Keep it asynchronous to GPU/video.
# GPU and video are deliberately NOT cut from one another: the known
# phase-dependent displayed-byte crossing must remain visible in TimeQuest.
set_clock_groups -asynchronous \
    -group [get_clocks {audio_clk}] \
    -group [get_clocks {gpu_clk vid_clk}]

# Harness data/reset ports intentionally have no invented board I/O delays or
# reset exceptions. TimeQuest must report the resulting unconstrained and
# recovery/removal limitations rather than turn them green by assertion.
