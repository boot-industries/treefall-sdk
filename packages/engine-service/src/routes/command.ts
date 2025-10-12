/**
 * Command execution endpoint
 */

import type { FastifyInstance, FastifyRequest, FastifyReply } from 'fastify';
import type { CommandRequest, CommandResponse } from '../types.js';

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

    // TODO (P1.DRIV.002): Link to Orpheus C++ core for actual command execution
    // For now, return a mock response indicating the command was received
    server.log.info({ type, payload, requestId }, 'Received command');

    try {
      // Mock implementation - will be replaced in P1.DRIV.002
      const result = await mockCommandHandler(type, payload);

      const response: CommandResponse = {
        success: true,
        requestId,
        result,
      };

      reply.code(200).send(response);
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

/**
 * Mock command handler - will be replaced with actual Orpheus SDK integration
 * in P1.DRIV.002 (TASK-018)
 */
async function mockCommandHandler(type: string, payload: unknown): Promise<unknown> {
  switch (type) {
    case 'LoadSession':
      return {
        sessionId: (payload as { sessionId?: string })?.sessionId || 'mock-session',
        status: 'loaded',
      };
    case 'RenderClick':
      return {
        status: 'rendering',
        message: 'Mock render started (C++ integration pending)',
      };
    default:
      throw new Error(`Unknown command type: ${type}`);
  }
}
