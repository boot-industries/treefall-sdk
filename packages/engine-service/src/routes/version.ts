/**
 * Version information endpoint
 */

import type { FastifyInstance } from 'fastify';
import type { VersionResponse } from '../types.js';

export async function versionRoute(server: FastifyInstance): Promise<void> {
  server.get('/version', async (request, reply) => {
    const response: VersionResponse = {
      service: 'orpheusd',
      serviceVersion: '0.1.0-alpha.0',
      sdkVersion: '0.1.0', // TODO: Read from C++ SDK once linked
      contractVersion: '0.1.0-alpha.0',
      nodeVersion: process.version,
    };

    reply.code(200).send(response);
  });
}
