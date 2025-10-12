/**
 * Orpheus Service Driver - Main Server
 */

import Fastify, { FastifyInstance } from 'fastify';
import fastifyWebsocket from '@fastify/websocket';
import type { ServiceConfig } from './types.js';
import { healthRoute } from './routes/health.js';
import { versionRoute } from './routes/version.js';
import { contractRoute } from './routes/contract.js';
import { commandRoute } from './routes/command.js';
import { setupWebSocket } from './websocket.js';

export async function createServer(config: ServiceConfig): Promise<FastifyInstance> {
  const server = Fastify({
    logger: {
      level: config.logLevel,
      transport: {
        target: 'pino-pretty',
        options: {
          colorize: true,
          translateTime: 'HH:MM:ss.l',
          ignore: 'pid,hostname',
        },
      },
    },
  });

  // Register WebSocket support
  await server.register(fastifyWebsocket);

  // Security warning for non-localhost binding
  if (config.host !== '127.0.0.1' && config.host !== 'localhost') {
    server.log.warn('');
    server.log.warn('⚠️  WARNING: Binding to %s exposes this service to your network.', config.host);
    server.log.warn('   Ensure firewall rules and authentication are properly configured.');
    server.log.warn('');
  }

  // CORS handling for localhost development
  server.addHook('onRequest', async (request, reply) => {
    // Only allow CORS for localhost origins
    const origin = request.headers.origin;
    if (origin && (origin.includes('localhost') || origin.includes('127.0.0.1'))) {
      reply.header('Access-Control-Allow-Origin', origin);
      reply.header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
      reply.header('Access-Control-Allow-Headers', 'Content-Type, Authorization');
      reply.header('Access-Control-Allow-Credentials', 'true');
    }

    // Handle preflight
    if (request.method === 'OPTIONS') {
      reply.code(204).send();
    }
  });

  // Authentication middleware (if token configured)
  if (config.authToken) {
    server.addHook('onRequest', async (request, reply) => {
      // Skip auth for health and version endpoints
      if (request.url === '/health' || request.url === '/version') {
        return;
      }

      const authHeader = request.headers.authorization;
      const token = authHeader?.replace('Bearer ', '');

      if (token !== config.authToken) {
        reply.code(401).send({
          error: {
            code: 'UNAUTHORIZED',
            message: 'Invalid or missing authentication token',
          },
        });
      }
    });
  }

  // Register HTTP routes
  await server.register(healthRoute);
  await server.register(versionRoute);
  await server.register(contractRoute);
  await server.register(commandRoute);

  // Register WebSocket route
  await server.register(setupWebSocket);

  return server;
}

export async function startServer(config: ServiceConfig): Promise<FastifyInstance> {
  const server = await createServer(config);

  try {
    await server.listen({ port: config.port, host: config.host });
    server.log.info(`Orpheus Service Driver listening on ${config.host}:${config.port}`);
    return server;
  } catch (err) {
    server.log.error(err);
    throw err;
  }
}
