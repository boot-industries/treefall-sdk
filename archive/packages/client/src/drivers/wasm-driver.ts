/**
 * WASM Driver Implementation
 *
 * Wraps @orpheus/engine-wasm Worker Client for broker compatibility
 */

import type { Command, Event } from '@orpheus/contract';
import type {
  IDriver,
  DriverCapabilities,
  HandshakeResult,
  CommandResponse,
  DriverConfig,
} from '../types.js';
import { DriverType, ConnectionStatus } from '../types.js';

// Dynamic import for WASM package (browser-only)
type OrpheusWorkerClient = {
  init(): Promise<void>;
  getVersion(): Promise<string>;
  loadSession(jsonString: string): Promise<void>;
  renderClick(bpm: number, bars: number): Promise<unknown>;
  getTempo(): Promise<number>;
  setTempo(bpm: number): Promise<void>;
  shutdown(): Promise<void>;
  onEvent(callback: (eventType: string, data: unknown) => void): () => void;
};

/**
 * WASM Driver
 *
 * Interfaces with Orpheus WASM module running in Web Worker
 */
export class WASMDriver implements IDriver {
  readonly type = DriverType.WASM;
  private _status = ConnectionStatus.Disconnected;
  private _capabilities: DriverCapabilities | null = null;
  private workerClient: OrpheusWorkerClient | null = null;
  private eventCallbacks: Set<(event: Event) => void> = new Set();
  private config: Required<NonNullable<DriverConfig['wasm']>>;

  constructor(config?: DriverConfig['wasm']) {
    this.config = {
      workerUrl: config?.workerUrl || '/orpheus-worker.js',
      baseUrl: config?.baseUrl || '/wasm/',
      integrity: config?.integrity || undefined,
      timeout: config?.timeout || 30000,
    };
  }

  get status(): ConnectionStatus {
    return this._status;
  }

  get capabilities(): DriverCapabilities | null {
    return this._capabilities;
  }

  async connect(): Promise<void> {
    if (this._status === ConnectionStatus.Connected) {
      return;
    }

    this._status = ConnectionStatus.Connecting;

    try {
      // Dynamic import @orpheus/engine-wasm
      // TypeScript: use type assertion since this is an optional peer dependency
      let wasmModule: any;
      try {
        wasmModule = await import('@orpheus/engine-wasm' as any);
      } catch (err) {
        throw new Error('Failed to load @orpheus/engine-wasm. Ensure it is installed.');
      }

      this.workerClient = new wasmModule.OrpheusWorkerClient({
        workerUrl: this.config.workerUrl,
        baseUrl: this.config.baseUrl,
        integrity: this.config.integrity as any,
        timeout: this.config.timeout,
      }) as OrpheusWorkerClient;

      // Initialize worker
      await this.workerClient.init();

      // Subscribe to events
      this.workerClient.onEvent((eventType, data) => {
        this.handleWorkerEvent(eventType, data);
      });

      // Perform handshake
      const handshake = await this.handshake();
      if (!handshake.success) {
        throw new Error(handshake.error || 'Handshake failed');
      }

      this._status = ConnectionStatus.Connected;
    } catch (error) {
      this._status = ConnectionStatus.Error;
      throw error;
    }
  }

  async disconnect(): Promise<void> {
    if (this.workerClient) {
      await this.workerClient.shutdown();
      this.workerClient = null;
    }

    this._status = ConnectionStatus.Disconnected;
    this._capabilities = null;
    this.eventCallbacks.clear();
  }

  async handshake(): Promise<HandshakeResult> {
    if (!this.workerClient) {
      return {
        success: false,
        capabilities: this.getDefaultCapabilities(),
        error: 'Worker client not initialized',
      };
    }

    try {
      const version = await this.workerClient.getVersion();

      this._capabilities = {
        commands: ['LoadSession', 'RenderClick', 'GetTempo', 'SetTempo'],
        events: ['SessionChanged', 'Heartbeat'],
        version,
        contractVersion: '0.1.0-alpha',
        supportsRealTimeEvents: true,
        metadata: {
          runtime: 'wasm',
          worker: true,
        },
      };

      return {
        success: true,
        capabilities: this._capabilities,
      };
    } catch (error) {
      return {
        success: false,
        capabilities: this.getDefaultCapabilities(),
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  async healthCheck(): Promise<boolean> {
    if (!this.workerClient || this._status !== ConnectionStatus.Connected) {
      return false;
    }

    try {
      await this.workerClient.getVersion();
      return true;
    } catch {
      return false;
    }
  }

  async execute(command: Command): Promise<CommandResponse> {
    if (!this.workerClient) {
      return {
        success: false,
        error: {
          code: 'WORKER_NOT_INITIALIZED',
          message: 'Worker client not initialized',
        },
      };
    }

    try {
      let result: unknown;

      switch (command.type) {
        case 'LoadSession':
          // LoadSession has 'path' field
          await this.workerClient.loadSession(JSON.stringify(command));
          result = { message: 'Session loaded' };
          break;

        case 'RenderClick':
          // RenderClick has optional bpm/bars fields
          result = await this.workerClient.renderClick(
            command.bpm || 120,
            command.bars || 4
          );
          break;

        default:
          return {
            success: false,
            error: {
              code: 'UNKNOWN_COMMAND',
              message: `Unknown command type: ${(command as any).type}`,
            },
          };
      }

      return {
        success: true,
        result,
      };
    } catch (error) {
      return {
        success: false,
        error: {
          code: 'EXECUTION_ERROR',
          message: error instanceof Error ? error.message : String(error),
        },
      };
    }
  }

  subscribe(callback: (event: Event) => void): () => void {
    this.eventCallbacks.add(callback);

    return () => {
      this.eventCallbacks.delete(callback);
    };
  }

  private handleWorkerEvent(eventType: string, data: unknown): void {
    // Convert worker event to Contract Event
    const event: Event = {
      type: eventType,
      timestamp: Date.now(),
      ...(data as Record<string, unknown>),
    } as Event;

    // Emit to all subscribers
    this.eventCallbacks.forEach(cb => cb(event));
  }

  private getDefaultCapabilities(): DriverCapabilities {
    return {
      commands: [],
      events: [],
      version: '0.0.0',
      supportsRealTimeEvents: false,
    };
  }
}
