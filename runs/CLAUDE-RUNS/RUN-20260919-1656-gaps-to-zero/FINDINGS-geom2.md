# FINDINGS — geom lane pass 2 (gz/geom2 94add368; merged e98d613f)

Transcribed by the coordinator. Register at the packet's tip: 46; on the merged tree: 42.

Commits: 3dae8f10 (R31 VDECODE-refusal deadlock), 5ee0b1dd (SKIN.NORM rate), d52ae6c0 (I46 closed, replay
rate), 95df2669 (R27, I12), 1f5ac60a (GROUP_SEQ review fix), b25e9992 (merge of 8736ba33), 5daf2256 (R25
SetEnvironment, I48 closed), 7c610a55 (R28 raster_state layout), 94add368 (VATTR review fix).

R31 clocks (Verilator, same fixture): skin 41.00 -> 10.00 clk/vtx; SKIN.NORM fork stalls 217 -> 0;
replay 56.46 -> 7.46 clk/view-tri (CLIP/ATTRPACK now set the rate); meshlet loop 1258 -> 305 clk. At the tier,
vertices take 1.2M clk (72%) and replay 597k (36%); the loop is SERIAL, so the sum is ~119% -> R47.

Review fixes (Qwen Q002/Q005, coordinator-verified):
* GROUP_SEQ: a hole with no job held is CARRIED to the next job, and is also excluded structurally
  (ASSETFETCH streams vertices only after the job handshake). New cases fail 6 checks on the old RTL.
* VATTR: the u/v join is now a handshake with a counter that fires. A dropped or out-of-store row POISONS the
  batch via REPLAY's poison input. Batch-boundary events go to the new batch. REPLAY cannot look up before the
  done_o-gated handle (tested; the u/v case fails without the wait).

Refused: I39/I24. The raster_state layout is ratified (zidl, PARAMBUF contract, zref, test), but the draw's
flags reach no meshlet until I36/R29; wiring cmd_draw_flags_o now would pair one meshlet with another draw's
cull mode. R21's terrain normal client did not fall out of I46 (terrain's arena lives in PROJ_SUBSYSTEM).
Decisions -> R48 (ALPHA_C = 1.0), R49 (tint/fog), R50 (emitter skipped).

False claims / instrument defects:
* The "-Mutant PASS" claimed at 153bd31e was FALSE: that mutant gate did not elaborate at that commit.
* The shared branch carried two stale tests (abi_gen expected 18 commands after R17's 19th;
  golden_abi_info failed 8 checks). Fixed.
* A zidl COMMENT edit moves the zidl SHA-256 in every capture header; goldens were regenerated through
  their producers (only SHA fields and the header CRC changed).
* Twice, cmake --build said "no work to do" after a same-size RTL swap and ran a stale model, once reporting
  a deliberately broken mutant as PASSING. Only deleting Vzhao_*.dir plus re-running the preset rebuilt it.
* PowerShell `r`/`rp` are aliases (Invoke-History / Remove-ItemProperty), so helper functions named R/Rp
  misfire.