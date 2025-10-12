/**
 * WebSocket event streaming
 */

import type { FastifyInstance, FastifyRequest } from 'fastify';
import type { WebSocket } from '@fastify/websocket';
import type { WebSocketMessage } from './types.js';

export async function setupWebSocket(server: FastifyInstance): Promise<void> {
  server.get('/ws', { websocket: true }, (socket: WebSocket, request: FastifyRequest) => {
    server.log.info({
      remoteAddress: request.socket.remoteAddress,
      clientCount: server.eventEmitter.getClientCount() + 1,
    }, 'WebSocket client connected');

    // Register this client with the event emitter
    server.eventEmitter.addClient(socket);

    // Send initial connection event
    const welcome: WebSocketMessage = {
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
    socket.send(JSON.stringify(welcome));

    // Handle incoming messages (ping/pong, subscriptions)
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
        // Future: Handle subscription filtering
        // if (message.type === 'subscribe') { ... }
      } catch (error) {
        server.log.error({ error }, 'Failed to parse WebSocket message');
      }
    });

    // Handle disconnection
    socket.on('close', () => {
      server.eventEmitter.removeClient(socket);
      server.log.info({
        clientCount: server.eventEmitter.getClientCount(),
      }, 'WebSocket client disconnected');
    });

    // Handle errors
    socket.on('error', (error: Error) => {
      server.eventEmitter.removeClient(socket);
      server.log.error({ error }, 'WebSocket error');
    });
  });
}
