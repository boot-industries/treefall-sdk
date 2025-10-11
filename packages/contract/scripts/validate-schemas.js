#!/usr/bin/env node
/**
 * Validate all JSON schemas using AJV
 * Part of TASK-016: Contract Schema Automation
 */

import Ajv from 'ajv';
import { readdirSync, readFileSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const SCHEMAS_DIR = join(__dirname, '..', 'schemas', 'v0.1.0-alpha');

const ajv = new Ajv({ allErrors: true, strict: true });

let errors = 0;

function validateSchemaFile(filePath) {
  try {
    const schema = JSON.parse(readFileSync(filePath, 'utf-8'));
    ajv.compile(schema);
    console.log(`✓ ${filePath}`);
  } catch (err) {
    console.error(`✗ ${filePath}: ${err.message}`);
    errors++;
  }
}

function walkDir(dir) {
  const entries = readdirSync(dir, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = join(dir, entry.name);
    if (entry.isDirectory()) {
      walkDir(fullPath);
    } else if (entry.name.endsWith('.json')) {
      validateSchemaFile(fullPath);
    }
  }
}

console.log('=== Validating Contract Schemas ===\n');
walkDir(SCHEMAS_DIR);

if (errors > 0) {
  console.error(`\n✗ ${errors} schema(s) failed validation`);
  process.exit(1);
} else {
  console.log('\n✓ All schemas valid');
}
