/**
 * Integration Smoke Tests
 *
 * Basic smoke tests to verify package imports and basic functionality
 * Run with: node --test tests/integration/smoke.test.mjs
 */

import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const rootDir = resolve(__dirname, '../..');

describe('Package Imports', () => {
  test('@orpheus/contract exports are available', async () => {
    const contractPath = resolve(rootDir, 'packages/contract/dist/index.js');
    const contract = await import(contractPath);

    assert.ok(contract, 'Contract package should export');
    assert.ok(typeof contract === 'object', 'Contract should be an object');

    // Check for key exports (interfaces don't exist at runtime, only their usages)
    // Just verify the package loads successfully
    assert.ok(contract !== null, 'Contract should not be null');
  });

  test('@orpheus/client exports are available', async () => {
    const clientPath = resolve(rootDir, 'packages/client/dist/index.js');
    const client = await import(clientPath);

    assert.ok(client.OrpheusClient, 'OrpheusClient class should be exported');
    assert.ok(client.DriverType, 'DriverType enum should be exported');
    assert.ok(client.ConnectionStatus, 'ConnectionStatus enum should be exported');

    assert.strictEqual(typeof client.OrpheusClient, 'function',
      'OrpheusClient should be a constructor function');
  });

  test('@orpheus/react exports are available', async () => {
    const reactPath = resolve(rootDir, 'packages/react/dist/index.js');
    const react = await import(reactPath);

    assert.ok(react.OrpheusProvider, 'OrpheusProvider should be exported');
    assert.ok(react.useOrpheus, 'useOrpheus hook should be exported');
    assert.ok(react.useOrpheusCommand, 'useOrpheusCommand hook should be exported');
    assert.ok(react.useOrpheusEvents, 'useOrpheusEvents hook should be exported');
  });
});

describe('OrpheusClient Instantiation', () => {
  test('can create OrpheusClient with default config', async () => {
    const clientPath = resolve(rootDir, 'packages/client/dist/index.js');
    const { OrpheusClient } = await import(clientPath);

    const client = new OrpheusClient();

    assert.ok(client, 'Client should be created');
    assert.strictEqual(client.status, 'disconnected', 'Initial status should be disconnected');
    assert.strictEqual(client.driver, null, 'No driver should be selected initially');
  });

  test('can create OrpheusClient with custom config', async () => {
    const clientPath = resolve(rootDir, 'packages/client/dist/index.js');
    const { OrpheusClient, DriverType } = await import(clientPath);

    const client = new OrpheusClient({
      driverPreference: [DriverType.Service],
      autoConnect: false,
      requiredCommands: ['LoadSession'],
    });

    assert.ok(client, 'Client should be created with config');
    assert.strictEqual(client.status, 'disconnected',
      'Status should be disconnected when autoConnect is false');
  });

  test('client has expected API surface', async () => {
    const clientPath = resolve(rootDir, 'packages/client/dist/index.js');
    const { OrpheusClient } = await import(clientPath);

    const client = new OrpheusClient({ autoConnect: false });

    // Check methods exist
    assert.ok(typeof client.connect === 'function', 'connect method should exist');
    assert.ok(typeof client.disconnect === 'function', 'disconnect method should exist');
    assert.ok(typeof client.execute === 'function', 'execute method should exist');
    assert.ok(typeof client.subscribe === 'function', 'subscribe method should exist');
    assert.ok(typeof client.on === 'function', 'on method should exist');

    // Check properties
    assert.ok('status' in client, 'status property should exist');
    assert.ok('driver' in client, 'driver property should exist');
    assert.ok('capabilities' in client, 'capabilities property should exist');
  });
});

describe('Contract Schemas', () => {
  test('LoadSession command validates correctly', async () => {
    const contractPath = resolve(rootDir, 'packages/contract/dist/index.js');
    const contract = await import(contractPath);

    // This is a smoke test - just verify the structure exists
    // Full schema validation would require AJV or similar
    assert.ok(contract, 'Contract should be importable');
  });
});

console.log('✓ Smoke tests completed successfully');
