#!/usr/bin/env node
/**
 * Generate MANIFEST.json with checksums of schema files
 * Part of TASK-097: Contract Manifest System
 */

import { createHash } from 'crypto';
import { readdirSync, readFileSync, writeFileSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const ROOT_DIR = join(__dirname, '..');
const SCHEMAS_DIR = join(ROOT_DIR, 'schemas');
const MANIFEST_PATH = join(ROOT_DIR, 'MANIFEST.json');

function hashDirectory(dir) {
  const hash = createHash('sha256');
  const files = [];

  function walkDir(currentDir) {
    const entries = readdirSync(currentDir, { withFileTypes: true }).sort((a, b) =>
      a.name.localeCompare(b.name)
    );
    for (const entry of entries) {
      const fullPath = join(currentDir, entry.name);
      if (entry.isDirectory()) {
        walkDir(fullPath);
      } else if (entry.name.endsWith('.json')) {
        files.push(fullPath);
      }
    }
  }

  walkDir(dir);

  // Hash all files in sorted order for determinism
  for (const file of files) {
    hash.update(readFileSync(file));
  }

  return hash.digest('hex');
}

console.log('=== Generating Contract Manifest ===\n');

const versions = [];
const versionDirs = readdirSync(SCHEMAS_DIR, { withFileTypes: true })
  .filter((d) => d.isDirectory())
  .map((d) => d.name)
  .sort();

for (const versionDir of versionDirs) {
  const versionPath = join(SCHEMAS_DIR, versionDir);
  const checksum = hashDirectory(versionPath);

  let status = 'alpha';
  if (versionDir.includes('-beta')) status = 'beta';
  else if (!versionDir.includes('-')) status = 'stable';

  const version = versionDir.replace(/^v/, '');
  versions.push({
    version,
    path: `schemas/${versionDir}`,
    checksum: `sha256:${checksum}`,
    status,
  });

  console.log(`✓ ${version}: ${checksum.substring(0, 16)}...`);
}

const manifest = {
  currentVersion: versions[versions.length - 1].version,
  versions,
};

writeFileSync(MANIFEST_PATH, JSON.stringify(manifest, null, 2) + '\n');
console.log(`\n✓ Manifest written to ${MANIFEST_PATH}`);
console.log(`  Current version: ${manifest.currentVersion}`);
