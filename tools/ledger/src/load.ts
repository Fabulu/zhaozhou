/**
 * Load + structural validation (YAML -> object -> Ajv, draft 2020-12).
 */
import * as fs from 'node:fs';
import * as path from 'node:path';
import { parse as parseYaml } from 'yaml';
import Ajv2020 from 'ajv/dist/2020';
import type { BlocksDoc, OpsDoc } from './types';

export interface Loaded<T> {
  doc: T;
  /** JSON Schema errors (empty when structurally valid). */
  schemaErrors: string[];
}

export function loadYaml<T>(file: string): T {
  const text = fs.readFileSync(file, 'utf8');
  const doc = parseYaml(text);
  if (doc === null || doc === undefined) {
    throw new Error(`empty YAML document: ${file}`);
  }
  return doc as T;
}

function makeAjv(): Ajv2020 {
  // strict:false — schemas use $defs/conditional shapes; we want plain validation.
  return new Ajv2020({ allErrors: true, strict: false });
}

export function validateWithSchema(instance: unknown, schemaFile: string): string[] {
  const schema = loadYaml<Record<string, unknown>>(schemaFile) as unknown as object;
  const ajv = makeAjv();
  const validate = ajv.compile(schema);
  const ok = validate(instance);
  if (ok) return [];
  const errs = validate.errors ?? [];
  return errs.map(
    (e) => `schema: ${e.instancePath || '(root)'} ${e.message ?? '(no message)'} [${e.keyword}]`
  );
}

export function loadLedger(designDir: string): { blocks: Loaded<BlocksDoc>; ops: Loaded<OpsDoc> } {
  const blocksDoc = loadYaml<BlocksDoc>(path.join(designDir, 'blocks.yml'));
  const opsDoc = loadYaml<OpsDoc>(path.join(designDir, 'ops.yml'));
  return {
    blocks: {
      doc: blocksDoc,
      schemaErrors: validateWithSchema(blocksDoc, path.join(designDir, 'schema', 'blocks.schema.json')),
    },
    ops: {
      doc: opsDoc,
      schemaErrors: validateWithSchema(opsDoc, path.join(designDir, 'schema', 'ops.schema.json')),
    },
  };
}
