/**
 * Event Emitter - Manages WebSocket event broadcasting
 */

import type { WebSocket } from '@fastify/websocket';
import type { WebSocketMessage } from '../types.js';

export interface OrpheusEvent {
  type: string;
  payload: unknown;
  timestamp?: number;
}

export class EventEmitter {
  private clients: Set<WebSocket> = new Set();

  /**
   * Register a WebSocket client for event broadcasts
   */
  addClient(client: WebSocket): void {
    this.clients.add(client);
  }

  /**
   * Unregister a WebSocket client
   */
  removeClient(client: WebSocket): void {
    this.clients.delete(client);
  }

  /**
   * Get count of connected clients
   */
  getClientCount(): number {
    return this.clients.size;
  }

  /**
   * Emit an event to all connected WebSocket clients
   */
  emit(event: OrpheusEvent): void {
    if (this.clients.size === 0) {
      // No clients connected, skip broadcasting
      return;
    }

    const message: WebSocketMessage = {
      type: 'event',
      payload: {
        type: event.type,
        payload: event.payload,
      },
      timestamp: event.timestamp || Date.now(),
    };

    const messageStr = JSON.stringify(message);

    // Broadcast to all connected clients
    const disconnected: WebSocket[] = [];
    let successCount = 0;

    for (const client of this.clients) {
      // Check if client is still open
      if (client.readyState === client.OPEN) {
        try {
          client.send(messageStr);
          successCount++;
        } catch (error) {
          console.error('Failed to send event to client:', error);
          disconnected.push(client);
        }
      } else {
        disconnected.push(client);
      }
    }

    // Log broadcast (only for non-heartbeat events to avoid spam)
    if (event.type !== 'Heartbeat') {
      console.log(`[EventEmitter] Broadcast ${event.type} to ${successCount}/${this.clients.size} clients`);
    }

    // Clean up disconnected clients
    for (const client of disconnected) {
      this.clients.delete(client);
    }
  }

  /**
   * Emit SessionChanged event (triggered after session operations)
   */
  emitSessionChanged(sessionData: {
    name: string;
    tempo?: number;
    trackCount?: number;
    clipCount?: number;
  }): void {
    this.emit({
      type: 'SessionChanged',
      payload: sessionData,
    });
  }

  /**
   * Emit Heartbeat event (for connection keepalive)
   */
  emitHeartbeat(): void {
    this.emit({
      type: 'Heartbeat',
      payload: {
        timestamp: Date.now(),
      },
    });
  }

  /**
   * Emit RenderProgress event (for render operations)
   */
  emitRenderProgress(progress: {
    command: string;
    percentComplete: number;
    estimatedTimeRemaining?: number;
  }): void {
    this.emit({
      type: 'RenderProgress',
      payload: progress,
    });
  }
}
