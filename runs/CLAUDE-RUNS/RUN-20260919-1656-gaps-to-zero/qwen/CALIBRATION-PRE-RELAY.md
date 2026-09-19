# Qwen calibration before the relay (via homeai worker_submit, code pasted inline)

| id | task | known answer? | result | verdict |
|---|---|---|---|---|
| cal-1 | 60-line CRC walker with ONE planted bug (fold8 where fold4 belongs) | yes | FOUND, with a correct concrete input and the tell (fold4 unused); 0 false positives; one minor misstatement in its "verified" list (early `last` refused via idx, not over_q) | verified 1/1 |
| cal-2 | real 170-line N-client HPS arbiter (`zhao_hps_arbiter_n`) | no (coordinator suspected a lost pulse) | HIGH: a one-cycle requester's pulse is dropped while another client owns the bridge. **VERIFIED against `zhao_cmd_dma.sv:524`** (M_HDR_REQ pulses once, then M_HDR_WAIT has no timeout). Suspects S1 (err-spin by a holder that de-asserts only on grant), S2 (write ports ungated during a read burst), S3 (bridge busy-release timing) handed to cmdmem for checking. About 8 min. | verified (the main finding); suspects pending |

From Q001 on, jobs go through `tools/qwen` (a relay copied from manafold-p16): files are read from disk, the prompt ceiling is 60k tokens, effort is xhigh, and max_tokens is set to the rest of the window.