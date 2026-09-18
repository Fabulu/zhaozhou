"""Exact arithmetic and cycle models for the proposed lighting rescue.

Independent engineering model, NOT a copy of the complete repository oracle,
NOT an HDL simulator, and NOT an area/timing measurement.
Pinned source reviewed: 18ef80e801b75aa5e375b841e236bc40baacc81d.
"""
from dataclasses import dataclass
from math import isqrt
from typing import Optional

U32 = (1 << 32) - 1
U64 = (1 << 64) - 1
I32_MIN = -(1 << 31)
I32_MAX = (1 << 31) - 1
ONE = 1 << 16

def sat32(x: int) -> int:
    return min(I32_MAX, max(I32_MIN, x))

def clamp01(x: int) -> int:
    return min(ONE, max(0, x))

def raw_direct(dot: int, den: int) -> int:
    if den == 0:
        return 0
    if not 0 < den <= U32:
        raise ValueError('denominator outside the qualified u32 domain')
    return sat32((dot + den // 2) // den)

@dataclass(frozen=True)
class DivState:
    rem: int
    work: int
    den: int
    neg: bool
    big: bool
    zero: bool
    tag: int

def div_seed(num: int, den: int, neg: bool = False, tag: int = 0) -> DivState:
    if not 0 <= num <= U64 or not 0 <= den <= U32:
        raise ValueError('divider expects u64 magnitude and u32 denominator')
    zero = den == 0
    big = not zero and (num >> 32) >= den
    # Bypass packets travel through the same pipeline; inert arithmetic is safe.
    return DivState(0 if zero or big else num >> 32,
                    0 if zero or big else num & U32,
                    1 if zero else den, bool(neg), big, zero, tag)

def div_step(s: DivState) -> DivState:
    t = (s.rem << 1) | (s.work >> 31)
    take = t >= s.den
    return DivState(t - s.den if take else t,
                    ((s.work << 1) | int(take)) & U32,
                    s.den, s.neg, s.big, s.zero, s.tag)

def div_finish(s: DivState) -> tuple[int, bool, bool, int]:
    if s.zero:
        return 0, False, True, s.tag
    if s.big:
        return (I32_MIN if s.neg else I32_MAX), True, False, s.tag
    q = -(s.work + int(s.rem != 0)) if s.neg else s.work
    return sat32(q), q < I32_MIN or q > I32_MAX, False, s.tag

def raw_div32(dot: int, den: int) -> int:
    h = dot + den // 2
    if abs(h) > U64:
        raise ValueError('rounded numerator exceeds qualified lighting domain')
    s = div_seed(abs(h), den, h < 0)
    for _ in range(32):
        s = div_step(s)
    return div_finish(s)[0]

class DivPipelineII2:
    """16 physical subtract steps, each reused in two phases.
    Output register finalizes the previous last stage at phase zero.
    Advance stops globally when the one-entry output is backpressured.
    tick returns (input_accepted, output_consumed); consumers see pre-edge output.
    """
    def __init__(self) -> None:
        self.reset()
    def reset(self) -> None:
        self.phase = 0
        self.stages: list[Optional[DivState]] = [None] * 16
        self.output: Optional[tuple[int, bool, bool, int]] = None
    def tick(self, incoming: Optional[DivState], ready: bool = True):
        out = self.output if ready else None
        ce = self.output is None or ready
        accepted = ce and self.phase == 0 and incoming is not None
        if ce:
            old = self.stages
            src = [incoming] + old[:-1] if self.phase == 0 else old
            self.output = div_finish(old[-1]) if self.phase == 0 and old[-1] else None
            self.stages = [div_step(s) if s is not None else None for s in src]
            self.phase ^= 1
        return accepted, out

@dataclass(frozen=True)
class RootState:
    rad: int
    rem: int
    root: int
    tag: int = 0

def root_step(s: RootState) -> RootState:
    t = (s.rem << 2) | (s.rad >> 62)
    trial = (s.root << 2) | 1
    take = t >= trial
    return RootState((s.rad << 2) & U64, t - trial if take else t,
                     (s.root << 1) | int(take), s.tag)

def root_exact(x: int) -> int:
    if not 0 <= x <= U64:
        raise ValueError('radicand must be u64')
    s = RootState(x, 0, 0)
    for _ in range(32):
        s = root_step(s)
    return s.root

class RootPipelineII8:
    """Four physical root steps, each reused for eight phases."""
    def __init__(self) -> None:
        self.reset()
    def reset(self) -> None:
        self.phase = 0
        self.stages: list[Optional[RootState]] = [None] * 4
        self.output: Optional[tuple[int, int]] = None
    def tick(self, incoming: Optional[RootState], ready: bool = True):
        out = self.output if ready else None
        ce = self.output is None or ready
        accepted = ce and self.phase == 0 and incoming is not None
        if ce:
            old = self.stages
            src = [incoming] + old[:-1] if self.phase == 0 else old
            self.output = (old[-1].root, old[-1].tag) if self.phase == 0 and old[-1] else None
            self.stages = [root_step(s) if s is not None else None for s in src]
            self.phase = (self.phase + 1) & 7
        return accepted, out

def prepare_render(n: tuple[int, int, int]) -> int:
    if any(x < I32_MIN or x > I32_MAX for x in n):
        raise ValueError('render normal outside s32')
    return isqrt(sum(x*x for x in n))

def narrow_skin_pair(n: tuple[int, int, int], mag: int) -> tuple[tuple[int,int,int], int]:
    # The producer's range reduction yields values within s32 and mag within u32.
    # Validate; never silently cast arbitrary s64 producer data to s32.
    if any(x < I32_MIN or x > I32_MAX for x in n) or not 0 <= mag <= U32:
        raise ValueError('unqualified skin pair')
    return n, mag

def render_ndl(n: tuple[int,int,int], l: tuple[int,int,int], detail: int = 0) -> int:
    d = prepare_render(n)
    if d == 0:
        return 0  # current GEOM.LIGHT direct-light behavior; ambient handled separately
    return clamp01(raw_div32(sum(a*b for a,b in zip(n,l)), d) + detail)

def creature_ndl(n: tuple[int,int,int], mag: int, l: tuple[int,int,int]) -> int:
    n, mag = narrow_skin_pair(n, mag)
    if mag == 0:
        return 0  # invalid producer handled outside lambert_from_world_normal
    return clamp01(raw_div32(sum(a*b for a,b in zip(n,l)), mag))

def colour_term(gain: int, response: int, gainw: int = 20) -> int:
    if not 0 <= gain < 1 << gainw or not 0 <= response <= ONE:
        raise ValueError('invalid colour product input')
    return (gain * response + (1 << 15)) >> 16

def issue_calendar(vertices: int = 120_000, lights: int = 4) -> dict:
    """Exact slot count for two dot/square lanes and three colour lanes.
    This is a schedule check, not a connected engine simulation.
    """
    if vertices < 1 or lights < 1:
        raise ValueError('positive workload required')
    terms = vertices * lights
    cycles = 2 * terms
    mult_slots = 2 * cycles
    dot_products = 3 * terms
    norm_products = 3 * vertices
    # Each term's second dot cycle has one free wide-product slot.
    normal_slots = terms
    norm_ii_slots = cycles // 8
    return {'vertices':vertices,'terms':terms,'issue_cycles':cycles,
            'wide_product_demand':dot_products+norm_products,
            'wide_product_capacity':mult_slots,
            'normal_square_slots':normal_slots,
            'normal_square_demand':norm_products,
            'root_capacity':norm_ii_slots,'root_demand':vertices,
            'colour_product_demand':6*terms,'colour_product_capacity':3*cycles,
            'passes_issue_resources':dot_products+norm_products <= mult_slots
              and norm_products <= normal_slots and vertices <= norm_ii_slots,
            'passes_raw_60hz_window':cycles <= 1_666_666,
            'passes_20pct_reserved_window':cycles <= 1_333_333}
