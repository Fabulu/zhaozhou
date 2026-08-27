// optable.test.ts — the committed C++ canonical-operation table must be
// byte-identical to what gen_optable.ts emits from OP_INFO today.
//
// This is the anti-drift gate for reference/include/zfield/generated/
// zfield_optable.hpp: the same write-if-changed discipline as the committed
// .hpp program wrappers. If this test fails, regenerate:
//   npm run build && node dist/src/field_ir/gen_optable.js --write
// and commit the result TOGETHER with the types.ts change that moved it.

import { test } from 'node:test';
import { strict as assert } from 'node:assert';
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { emitOptableHpp } from '../src/field_ir/gen_optable.js';

const HERE = dirname(fileURLToPath(import.meta.url));
// dist/tests -> zhaozhou repo root is three levels up from the dist file.
const OUT = resolve(HERE, '..', '..', '..', 'reference', 'include', 'zfield', 'generated',
                    'zfield_optable.hpp');

test('committed zfield_optable.hpp matches OP_INFO regeneration byte-for-byte', () => {
  const want = emitOptableHpp();
  const have = readFileSync(OUT, 'utf-8');
  assert.equal(have, want,
               'zfield_optable.hpp is STALE against compiler OP_INFO - regenerate and commit');
});
