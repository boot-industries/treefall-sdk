/**
 * @file worker-client.ts
 * @brief Main-thread client for communicating with Orpheus WASM Worker
 */

import type { WorkerCommand, WorkerResponse } from './worker.js';
import type { IntegrityManifest } from './loader.js';

/**
 * Event callback type
 */
export type EventCallback = (eventType: string, data: unknown) => void;

/**
 * Worker client configuration
 */
export interface WorkerClientConfig {
  workerUrl: string;
  baseUrl: string;
  integrity?: IntegrityManifest;
  timeout?: number;
}

/**
 * Main-thread client for Orpheus WASM Worker
 *
 * Manages worker lifecycle and provides async command interface
 */
export class OrpheusWorkerClient {
  private worker: Worker | null = null;
  private pendingCommands: Map<string, {
    resolve: (value: unknown) => void;
    reject: (error: Error) => void;
    timeout: ReturnType<typeof setTimeout>;
  }> = new Map();
  private eventCallbacks: Set<EventCallback> = new Set();
  private config: WorkerClientConfig;
  private isInitialized = false;

  constructor(config: WorkerClientConfig) {
    this.config = {
      timeout: 30000, // 30s default
      ...config,
    };
  }

  /**
   * Initialize worker and WASM module
   */
  async init(): Promise<void> {
    if (this.isInitialized) {
      throw new Error('Worker already initialized');
    }

    // Create worker
    this.worker = new Worker(this.config.workerUrl, { type: 'module' });

    // Set up message handler
    this.worker.onmessage = (event: MessageEvent<WorkerResponse>) => {
      this.handleWorkerMessage(event.data);
    };

    this.worker.onerror = (error) => {
      console.error('[OrpheusWorkerClient] Worker error:', error);
      this.rejectAllPending(new Error('Worker error'));
    };

    // Send init command
    await this.sendCommand('init', {
      baseUrl: this.config.baseUrl,
      integrity: this.config.integrity,
    });

    this.isInitialized = true;
  }

  /**
   * Get Orpheus version
   */
  async getVersion(): Promise<string> {
    const result = await this.sendCommand('getVersion', {}) as { version: string };
    return result.version;
  }

  /**
   * Load session from JSON
   */
  async loadSession(jsonString: string): Promise<void> {
    await this.sendCommand('loadSession', { jsonString });
  }

  /**
   * Render click track
   */
  async renderClick(bpm: number, bars: number): Promise<unknown> {
    return await this.sendCommand('renderClick', { bpm, bars });
  }

  /**
   * Get current tempo
   */
  async getTempo(): Promise<number> {
    const result = await this.sendCommand('getTempo', {}) as { tempo: number };
    return result.tempo;
  }

  /**
   * Set tempo
   */
  async setTempo(bpm: number): Promise<void> {
    await this.sendCommand('setTempo', { bpm });
  }

  /**
   * Shutdown worker
   */
  async shutdown(): Promise<void> {
    if (!this.worker) {
      return;
    }

    try {
      await this.sendCommand('shutdown', {});
    } finally {
      this.worker.terminate();
      this.worker = null;
      this.isInitialized = false;
      this.rejectAllPending(new Error('Worker terminated'));
    }
  }

  /**
   * Subscribe to events from worker
   */
  onEvent(callback: EventCallback): () => void {
    this.eventCallbacks.add(callback);

    // Return unsubscribe function
    return () => {
      this.eventCallbacks.delete(callback);
    };
  }

  /**
   * Send command to worker and wait for response
   */
  private sendCommand(type: string, data: unknown): Promise<unknown> {
    if (!this.worker) {
      return Promise.reject(new Error('Worker not initialized'));
    }

    return new Promise((resolve, reject) => {
      const id = crypto.randomUUID();

      // Set up timeout
      const timeout = setTimeout(() => {
        this.pendingCommands.delete(id);
        reject(new Error(`Command '${type}' timed out after ${this.config.timeout}ms`));
      }, this.config.timeout);

      // Store pending command
      this.pendingCommands.set(id, { resolve, reject, timeout });

      // Send command
      const command: WorkerCommand = { id, type, data };
      this.worker!.postMessage(command);
    });
  }

  /**
   * Handle message from worker
   */
  private handleWorkerMessage(response: WorkerResponse): void {
    const { id, type, data, error } = response;

    if (type === 'event') {
      // Emit event to all subscribers
      const eventData = data as { eventType: string };
      this.eventCallbacks.forEach(cb => cb(eventData.eventType, eventData));
      return;
    }

    // Handle command response
    const pending = this.pendingCommands.get(id);
    if (!pending) {
      console.warn(`[OrpheusWorkerClient] Received response for unknown command ID: ${id}`);
      return;
    }

    // Clear timeout
    clearTimeout(pending.timeout);
    this.pendingCommands.delete(id);

    // Resolve or reject
    if (type === 'success') {
      pending.resolve(data);
    } else if (type === 'error') {
      pending.reject(new Error(error || 'Unknown error'));
    }
  }

  /**
   * Reject all pending commands
   */
  private rejectAllPending(error: Error): void {
    this.pendingCommands.forEach(pending => {
      clearTimeout(pending.timeout);
      pending.reject(error);
    });
    this.pendingCommands.clear();
  }
}
