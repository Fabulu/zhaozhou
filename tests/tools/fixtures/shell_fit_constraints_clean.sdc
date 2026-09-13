create_clock -name gpu_clk -period 10.000 [get_ports {gpu_clk}]
create_clock -name vid_clk -period 20.000 [get_ports {vid_clk}]
create_clock -name audio_clk -period 40.000 [get_ports {audio_clk}]
