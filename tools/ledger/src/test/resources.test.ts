/**
 * V23 resource census (src/resources.ts).
 *
 * The census reads reports/synthesis/zhao_block_fit.json and prints three
 * totals against device capacity, failing ONLY when a single block exceeds the
 * whole device. Both halves of that need holding in place:
 *
 *   - the totals must be right, because they are the number people act on;
 *   - the failure must fire only on the one condition that cannot be an
 *     artifact of summing, because a false gate is worse than no gate.
 *
 * It was verified by hand once, by faking a row to 200 DSPs. That is not a
 * regression test.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import { resourceCensus } from '../resources';

function row(over: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    module: 'zhao_thing',
    status: 'ok',
    dspBlocks: 4,
    alms: 1000,
    ramBlocks: 2,
    dspBlocksAvailable: 112,
    almsAvailable: 41910,
    ramBlocksAvailable: 553,
    ...over,
  };
}

test('census: sums each resource and takes capacity from the rows', () => {
  const c = resourceCensus({
    blocks: [row({ module: 'a', dspBlocks: 10 }), row({ module: 'b', dspBlocks: 7 })],
  });
  assert.ok(c);
  assert.equal(c.measured, 2);
  assert.equal(c.dsp.total, 17);
  assert.equal(c.dsp.capacity, 112);
  assert.equal(c.alm.total, 2000);
  assert.equal(c.m10k.total, 4);
});

test('census: rows that did not fit are excluded entirely', () => {
  // a timeout or a failed fit has no numbers; counting it as zero would make a
  // subsystem look cheaper than it is, which is the wrong direction to be wrong
  const c = resourceCensus({
    blocks: [
      row({ module: 'ok', dspBlocks: 5 }),
      row({ module: 'timeout', status: 'timeout', dspBlocks: 0 }),
      row({ module: 'failed', status: 'failed:quartus_fit.exe', dspBlocks: 0 }),
    ],
  });
  assert.ok(c);
  assert.equal(c.measured, 1);
  assert.equal(c.dsp.total, 5);
});

test('census: worst is sorted descending and capped at three', () => {
  const c = resourceCensus({
    blocks: [
      row({ module: 'small', dspBlocks: 1 }),
      row({ module: 'huge', dspBlocks: 79 }),
      row({ module: 'big', dspBlocks: 72 }),
      row({ module: 'mid', dspBlocks: 33 }),
    ],
  });
  assert.ok(c);
  assert.deepEqual(
    c.dsp.worst.map((w) => w.module),
    ['huge', 'big', 'mid']
  );
});

test('census: a total over capacity does NOT fail — it is an upper bound', () => {
  // 41 blocks summing to 327 DSPs against 112 is the real situation, and it is
  // not an error: per-block fits share nothing. Gating here would be a false
  // gate, and a false gate trains people to pass it instead of fixing the
  // design.
  const c = resourceCensus({
    blocks: [row({ module: 'a', dspBlocks: 79 }), row({ module: 'b', dspBlocks: 72 })],
  });
  assert.ok(c);
  assert.equal(c.dsp.total, 151);
  assert.ok(c.dsp.total > c.dsp.capacity);
  assert.deepEqual(c.dsp.overCapacityBlocks, []);
});

test('census: a SINGLE block over device capacity is flagged, per resource', () => {
  const c = resourceCensus({
    blocks: [row({ module: 'unplaceable', dspBlocks: 200 }), row({ module: 'fine', dspBlocks: 4 })],
  });
  assert.ok(c);
  assert.deepEqual(c.dsp.overCapacityBlocks, [{ module: 'unplaceable', n: 200 }]);
  assert.deepEqual(c.alm.overCapacityBlocks, []);
});

test('census: the ALM and M10K lines flag independently of DSP', () => {
  const c = resourceCensus({
    blocks: [row({ module: 'fat', alms: 99999, dspBlocks: 1, ramBlocks: 1 })],
  });
  assert.ok(c);
  assert.deepEqual(c.alm.overCapacityBlocks, [{ module: 'fat', n: 99999 }]);
  assert.deepEqual(c.dsp.overCapacityBlocks, []);
  assert.deepEqual(c.m10k.overCapacityBlocks, []);
});

test('census: no measured rows yields null rather than a zero report', () => {
  // a report of all-timeouts must not print "0 DSPs against 112" and look healthy
  assert.equal(resourceCensus({ blocks: [row({ status: 'timeout' })] }), null);
  assert.equal(resourceCensus({ blocks: [] }), null);
  assert.equal(resourceCensus(null), null);
});

test('census: a zero capacity cannot manufacture an over-capacity claim', () => {
  // an older report without the *Available fields must not make every block
  // look unplaceable
  const c = resourceCensus({
    blocks: [
      {
        module: 'old',
        status: 'ok',
        dspBlocks: 8,
        alms: 500,
        ramBlocks: 0,
      },
    ],
  });
  assert.ok(c);
  assert.equal(c.dsp.capacity, 0);
  assert.deepEqual(c.dsp.overCapacityBlocks, []);
});
