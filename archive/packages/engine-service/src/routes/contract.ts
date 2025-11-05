/**
 * Contract schema endpoint
 */

import type { FastifyInstance } from 'fastify';
import type { ContractResponse } from '../types.js';

export async function contractRoute(server: FastifyInstance): Promise<void> {
  server.get('/contract', async (request, reply) => {
    // List available command and event types from @orpheus/contract
    const response: ContractResponse = {
      version: '0.1.0-alpha.0',
      commands: [
        'LoadSession',
        'RenderClick',
        // More commands will be added as they are implemented
      ],
      events: [
        'SessionChanged',
        'Heartbeat',
        // More events will be added as they are implemented
      ],
      schemaUrl: 'https://github.com/your-org/orpheus-sdk/tree/main/packages/contract/schemas',
    };

    reply.code(200).send(response);
  });
}
