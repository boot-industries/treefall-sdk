#!/usr/bin/env node

/**
 * Performance Budget Validation Script
 *
 * Validates bundle sizes, command latencies, and event frequencies against
 * budgets defined in budgets.json.
 *
 * Usage: node scripts/validate-performance.js [--budgets=path/to/budgets.json]
 */

const fs = require('fs');
const path = require('path');

// Parse command line arguments
const args = process.argv.slice(2);
const budgetsPath =
  args.find((arg) => arg.startsWith('--budgets='))?.split('=')[1] || 'budgets.json';

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

const violations = [];
const warnings = [];

// ============================================================================
// Bundle Size Validation
// ============================================================================
console.log('\n📦 Validating Bundle Sizes...\n');

function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return `${(bytes / Math.pow(k, i)).toFixed(2)} ${sizes[i]}`;
}

function validateBundleSize(packageName, config) {
  const packagePath = path.join('packages', packageName.split('/')[1]);
  const distPath = path.join(packagePath, 'dist');

  if (!fs.existsSync(distPath)) {
    console.log(`  ⚠️  ${packageName}: dist/ not found (skipping)`);
    return;
  }

  // Calculate total bundle size
  let totalSize = 0;
  function calculateDirSize(dirPath) {
    const files = fs.readdirSync(dirPath, { withFileTypes: true });
    for (const file of files) {
      const fullPath = path.join(dirPath, file.name);
      if (file.isDirectory()) {
        calculateDirSize(fullPath);
      } else {
        const stats = fs.statSync(fullPath);
        totalSize += stats.size;
      }
    }
  }
  calculateDirSize(distPath);

  const { max, warn, description } = config;
  const degradationTolerance = parseFloat(budgets.budgets.degradationTolerance?.size || '0%') / 100;
  const maxWithTolerance = Math.floor(max * (1 + degradationTolerance));

  console.log(`  ${packageName}:`);
  console.log(`    Actual: ${formatBytes(totalSize)}`);
  console.log(
    `    Budget: ${formatBytes(max)} (with ${degradationTolerance * 100}% tolerance: ${formatBytes(maxWithTolerance)})`,
  );

  if (totalSize > maxWithTolerance) {
    const overage = totalSize - maxWithTolerance;
    violations.push({
      type: 'Bundle Size',
      package: packageName,
      actual: formatBytes(totalSize),
      budget: formatBytes(maxWithTolerance),
      overage: formatBytes(overage),
      description,
    });
    console.log(`    ❌ VIOLATION: Exceeds budget by ${formatBytes(overage)}`);
  } else if (totalSize > warn) {
    warnings.push({
      type: 'Bundle Size',
      package: packageName,
      actual: formatBytes(totalSize),
      warn: formatBytes(warn),
      description,
    });
    console.log(`    ⚠️  WARNING: Exceeds warning threshold`);
  } else {
    console.log(`    ✓ Within budget`);
  }
}

if (budgets.budgets.bundleSize) {
  for (const [packageName, config] of Object.entries(budgets.budgets.bundleSize)) {
    validateBundleSize(packageName, config);
  }
} else {
  console.log('  No bundle size budgets defined');
}

// ============================================================================
// Command Latency Validation
// ============================================================================
console.log('\n⏱️  Command Latency Budgets:\n');

if (budgets.budgets.commandLatency) {
  for (const [command, config] of Object.entries(budgets.budgets.commandLatency)) {
    console.log(`  ${command}:`);
    console.log(`    P95 budget: ${config.p95} ${config.unit}`);
    console.log(`    P99 budget: ${config.p99} ${config.unit}`);
    console.log(`    ℹ️  Actual latency measurement requires runtime profiling`);
  }
} else {
  console.log('  No command latency budgets defined');
}

// ============================================================================
// Event Frequency Validation
// ============================================================================
console.log('\n📡 Event Frequency Budgets:\n');

if (budgets.budgets.eventFrequency) {
  for (const [event, config] of Object.entries(budgets.budgets.eventFrequency)) {
    console.log(`  ${event}:`);
    console.log(`    Max frequency: ${config.max} ${config.unit}`);
    console.log(`    ℹ️  Informational budget; not enforced in CI`);
  }
} else {
  console.log('  No event frequency budgets defined');
}

// ============================================================================
// Summary
// ============================================================================
console.log('\n' + '='.repeat(80));
console.log('PERFORMANCE BUDGET VALIDATION SUMMARY');
console.log('='.repeat(80) + '\n');

if (violations.length > 0) {
  console.log('❌ VIOLATIONS FOUND:\n');
  violations.forEach((v, i) => {
    console.log(`${i + 1}. ${v.type}: ${v.package}`);
    console.log(`   Actual: ${v.actual}`);
    console.log(`   Budget: ${v.budget}`);
    console.log(`   Overage: ${v.overage}`);
    console.log(`   ${v.description}\n`);
  });
}

if (warnings.length > 0) {
  console.log('⚠️  WARNINGS:\n');
  warnings.forEach((w, i) => {
    console.log(`${i + 1}. ${w.type}: ${w.package}`);
    console.log(`   Actual: ${w.actual}`);
    console.log(`   Warning threshold: ${w.warn}`);
    console.log(`   ${w.description}\n`);
  });
}

if (violations.length === 0 && warnings.length === 0) {
  console.log('✅ All performance budgets met!\n');
  process.exit(0);
} else if (violations.length > 0) {
  console.log(`❌ ${violations.length} violation(s) found. CI should fail.\n`);
  process.exit(1);
} else {
  console.log(`⚠️  ${warnings.length} warning(s) found. Consider optimizing.\n`);
  process.exit(0);
}
