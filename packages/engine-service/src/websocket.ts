/**
 * WebSocket event streaming
 */

import type { FastifyInstance, FastifyRequest } from 'fastify';
import type { WebSocket } from '@fastify/websocket';
import type { WebSocketMessage } from './types.js';

export async function setupWebSocket(server: FastifyInstance): Promise<void> {
  server.get('/ws', { websocket: true }, (socket: WebSocket, request: FastifyRequest) => {

    server.log.info({ remoteAddress: request.socket.remoteAddress }, 'WebSocket client connected');

    // Send initial heartbeat
    const heartbeat: WebSocketMessage = {
      type: 'event',
      payload: {
        type: 'Heartbeat',
        payload: {
          timestamp: Date.now(),
          status: 'connected',
        },
      },
      timestamp: Date.now(),
    };
    socket.send(JSON.stringify(heartbeat));

    // Set up periodic heartbeat
    const heartbeatInterval = setInterval(() => {
      if (socket.readyState === socket.OPEN) {
        const msg: WebSocketMessage = {
          type: 'event',
          payload: {
            type: 'Heartbeat',
            payload: {
              timestamp: Date.now(),
            },
          },
          timestamp: Date.now(),
        };
        socket.send(JSON.stringify(msg));
      }
    }, 10000); // Every 10 seconds

    // Handle incoming messages (ping/pong)
    socket.on('message', (data: Buffer) => {
      try {
        const message = JSON.parse(data.toString()) as WebSocketMessage;

        if (message.type === 'ping') {
          const pong: WebSocketMessage = {
            type: 'pong',
            timestamp: Date.now(),
          };
          socket.send(JSON.stringify(pong));
        }
      } catch (error) {
        server.log.error({ error }, 'Failed to parse WebSocket message');
      }
    });

    // Handle disconnection
    socket.on('close', () => {
      clearInterval(heartbeatInterval);
      server.log.info('WebSocket client disconnected');
    });

    // Handle errors
    socket.on('error', (error: Error) => {
      clearInterval(heartbeatInterval);
      server.log.error({ error }, 'WebSocket error');
    });

    // TODO (P1.DRIV.003): Integrate with Orpheus SDK event system
    // - Listen for SessionChanged events
    // - Listen for transport state changes
    // - Emit events to all connected WebSocket clients
  });
}
