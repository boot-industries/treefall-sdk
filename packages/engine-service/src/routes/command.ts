/**
 * Command execution endpoint
 */

import type { FastifyInstance, FastifyRequest, FastifyReply } from 'fastify';
import type { CommandRequest, CommandResponse } from '../types.js';
import { executeOrpheusCommand } from '../orpheus/minhost-executor.js';

export async function commandRoute(server: FastifyInstance): Promise<void> {
  server.post<{ Body: CommandRequest }>('/command', async (request: FastifyRequest<{ Body: CommandRequest }>, reply: FastifyReply) => {
    const { type, payload, requestId } = request.body;

    // Validate command structure
    if (!type || typeof type !== 'string') {
      const response: CommandResponse = {
        success: false,
        requestId,
        error: {
          code: 'INVALID_COMMAND',
          message: 'Command type is required and must be a string',
        },
      };
      return reply.code(400).send(response);
    }

    server.log.info({ type, payload, requestId }, 'Executing command via Orpheus SDK');

    try {
      // Execute command via orpheus_minhost bridge
      const response = await executeOrpheusCommand(
        { type, payload, requestId },
        { timeout: 60000 } // 60 second timeout
      );

      // Emit events for successful command execution
      if (response.success && response.result) {
        if (type === 'LoadSession') {
          // Emit SessionChanged event after LoadSession
          const result = response.result as any;
          if (result.session) {
            server.eventEmitter.emitSessionChanged({
              name: result.session.name || 'Unnamed Session',
              tempo: result.session.tempo_bpm,
              trackCount: result.session.tracks?.loaded || 0,
              clipCount: result.session.clips || 0,
            });
          }
        }
        // Future: Add events for RenderClick, transport changes, etc.
      }

      reply.code(response.success ? 200 : 500).send(response);
    } catch (error) {
      const errorMessage = error instanceof Error ? error.message : 'Unknown error';
      const response: CommandResponse = {
        success: false,
        requestId,
        error: {
          code: 'COMMAND_FAILED',
          message: errorMessage,
        },
      };

      reply.code(500).send(response);
    }
  });
}
