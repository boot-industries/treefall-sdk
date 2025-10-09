import assert from 'node:assert';
import { initializeShmui } from '../src/index.js';

// Placeholder smoke test to ensure the placeholder implementation is wired correctly.
try {
  initializeShmui();
  assert.ok(true);
} catch (error) {
  assert.fail(`initializeShmui threw an error: ${error}`);
}
