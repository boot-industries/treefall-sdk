#!/usr/bin/env node

/**
 * Performance Budget Validation Script
 *
 * Every budget declared in budgets.json names the CTest gate that enforces it
 * through its `enforcedBy` field. This script proves three things for each
 * declared budget:
 *
 *   1. the gate is registered in the repository's CTest inventory,
 *   2. the gate is measurable in the requested build tree, and
 *   3. the gate passes.
 *
 * A budget that cannot be measured is a failure, never a pass. Budgets that
 * nothing enforces are not declared, because an unenforced budget is a claim
 * without a mechanism.
 *
 * Usage: node scripts/validate-performance.js [--budgets=path/to/budgets.json]
 *                                              [--build-dir=build]
 */

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const repoRoot = path.resolve(__dirname, '..');

function optionValue(flag) {
  const prefix = `${flag}=`;
  const match = process.argv.slice(2).find((arg) => arg.startsWith(prefix));
  return match === undefined ? undefined : match.slice(prefix.length);
}

const budgetsPath = optionValue('--budgets') ?? path.resolve(repoRoot, 'budgets.json');
const buildDir = optionValue('--build-dir') ?? 'build';

// Load budgets
let budgets;
try {
  const budgetsContent = fs.readFileSync(budgetsPath, 'utf8');
  budgets = JSON.parse(budgetsContent);
  console.log(`✓ Loaded budgets from ${budgetsPath}`);
} catch (error) {
  console.error(`❌ Failed to load budgets from ${budgetsPath}:`, error.message);
  process.exit(1);
}

const declared = budgets.budgets ?? {};
const budgetNames = Object.keys(declared);
if (budgetNames.length === 0) {
  console.error('❌ budgets.json declares no budgets.');
  process.exit(1);
}

/**
 * Collect the CTest test names registered by the repository's test tree.
 * Both the single-line `add_test(NAME foo ...)` form and the multi-line
 * `add_test(\n  NAME foo\n  ...)` form are recognised.
 */
function collectRegisteredTests() {
  const testsRoot = path.join(repoRoot, 'tests');
  const cmakeFiles = [];
  const rootManifest = path.join(testsRoot, 'CMakeLists.txt');
  if (fs.existsSync(rootManifest)) {
    cmakeFiles.push(rootManifest);
  }
  for (const entry of fs.readdirSync(testsRoot, { withFileTypes: true })) {
    if (!entry.isDirectory()) {
      continue;
    }
    const candidate = path.join(testsRoot, entry.name, 'CMakeLists.txt');
    if (fs.existsSync(candidate)) {
      cmakeFiles.push(candidate);
    }
  }

  const registered = new Set();
  const pattern = /add_test\s*\(\s*NAME\s+([A-Za-z0-9_.-]+)/g;
  for (const file of cmakeFiles) {
    const text = fs.readFileSync(file, 'utf8');
    let match;
    while ((match = pattern.exec(text)) !== null) {
      registered.add(match[1]);
    }
  }
  return registered;
}

const registeredTests = collectRegisteredTests();

console.log('\n' + '='.repeat(80));
console.log('PERFORMANCE BUDGET VALIDATION');
console.log('='.repeat(80) + '\n');

const failures = [];

for (const name of budgetNames) {
  const config = declared[name] ?? {};
  const gate = config.enforcedBy;

  if (typeof gate !== 'string' || gate.length === 0) {
    console.log(`❌ ${name}: no enforcing gate declared (missing "enforcedBy")`);
    failures.push(name);
    continue;
  }

  if (!registeredTests.has(gate)) {
    console.log(`❌ ${name}: ${gate} is not registered`);
    failures.push(name);
    continue;
  }

  if (!fs.existsSync(buildDir)) {
    console.log(
      `❌ ${name}: ${gate} is not measurable in this configuration (build tree "${buildDir}" does not exist)`,
    );
    failures.push(name);
    continue;
  }

  const ctest = spawnSync(
    'ctest',
    ['--test-dir', buildDir, '-R', `^${gate}$`, '--output-on-failure'],
    { encoding: 'utf8' },
  );

  if (ctest.stdout) {
    process.stdout.write(ctest.stdout);
  }
  if (ctest.stderr) {
    process.stderr.write(ctest.stderr);
  }

  if (ctest.error) {
    console.log(`❌ ${name}: ${gate} is not measurable in this configuration (${ctest.error.message})`);
    failures.push(name);
    continue;
  }

  // A gate that skipped did not measure the budget. Reporting that as a pass
  // would repeat the defect F-07 exists to remove.
  const skipped = new RegExp(`^.*${gate.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}.*Skipped`, 'm').test(
    ctest.stdout ?? '',
  );

  if (ctest.status === 0 && !skipped) {
    console.log(`✅ ${name}: ${gate} passed`);
  } else if (skipped) {
    console.log(`❌ ${name}: ${gate} is not measurable in this configuration`);
    failures.push(name);
  } else {
    console.log(`❌ ${name}: ${gate} failed`);
    failures.push(name);
  }
}

console.log('\n' + '='.repeat(80));

if (failures.length === 0) {
  console.log('✅ All performance budgets met!\n');
  process.exit(0);
}

console.log(`❌ ${failures.length} budget(s) not met.\n`);
process.exit(1);
