#!/usr/bin/env node
/**
 * Orpheus Service Driver CLI
 */

import { Command } from 'commander';
import { startServer } from '../server.js';
import type { ServiceConfig } from '../types.js';

const program = new Command();

program
  .name('orpheusd')
  .description('Orpheus Service Driver - HTTP/WebSocket service for Orpheus SDK')
  .version('0.1.0-alpha.0')
  .option('-p, --port <port>', 'Port to listen on', '8080')
  .option('-h, --host <host>', 'Host to bind to (default: 127.0.0.1)', '127.0.0.1')
  .option('--log-level <level>', 'Log level: trace, debug, info, warn, error', 'info')
  .option('--auth-token <token>', 'Require authentication token')
  .parse(process.argv);

const options = program.opts();

const config: ServiceConfig = {
  port: parseInt(options.port, 10),
  host: options.host,
  logLevel: options.logLevel as ServiceConfig['logLevel'],
  authToken: options.authToken,
};

// Validate port
if (isNaN(config.port) || config.port < 1 || config.port > 65535) {
  console.error(`Error: Invalid port number "${options.port}". Must be between 1 and 65535.`);
  process.exit(1);
}

// Validate log level
const validLogLevels: ServiceConfig['logLevel'][] = ['trace', 'debug', 'info', 'warn', 'error'];
if (!validLogLevels.includes(config.logLevel)) {
  console.error(`Error: Invalid log level "${config.logLevel}". Must be one of: ${validLogLevels.join(', ')}`);
  process.exit(1);
}

let server: Awaited<ReturnType<typeof startServer>> | null = null;

// Graceful shutdown handler
const shutdown = async (signal: string) => {
  console.log(`\nReceived ${signal}, shutting down gracefully...`);
  if (server) {
    await server.close();
    console.log('Server closed');
  }
  process.exit(0);
};

process.on('SIGTERM', () => shutdown('SIGTERM'));
process.on('SIGINT', () => shutdown('SIGINT'));

// Start server
(async () => {
  try {
    server = await startServer(config);
  } catch (error) {
    console.error('Failed to start server:', error);
    process.exit(1);
  }
})();
