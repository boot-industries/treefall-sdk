/**
 * Health check endpoint
 */

import type { FastifyInstance } from 'fastify';
import type { HealthResponse } from '../types.js';

const startTime = Date.now();

export async function healthRoute(server: FastifyInstance): Promise<void> {
  server.get('/health', async (request, reply) => {
    const uptime = (Date.now() - startTime) / 1000;

    const response: HealthResponse = {
      status: 'ok',
      uptime,
      version: '0.1.0-alpha.0',
      timestamp: Date.now(),
    };

    reply.code(200).send(response);
  });
}
