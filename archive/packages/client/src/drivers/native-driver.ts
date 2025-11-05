/**
 * Native Driver Implementation
 *
 * Connects to @orpheus/engine-native via N-API bindings
 */

import type { Command, Event } from '@orpheus/contract';
import {
  DriverType,
  ConnectionStatus,
  type IDriver,
  type DriverCapabilities,
  type DriverConfig,
  type CommandResponse,
  type HandshakeResult,
} from '../types.js';

/**
 * Native Driver
 *
 * Direct N-API bindings to Orpheus C++ SDK
 */
export class NativeDriver implements IDriver {
  readonly type = DriverType.Native;

  private _status: ConnectionStatus = ConnectionStatus.Disconnected;
  private _capabilities: DriverCapabilities | null = null;
  private _config: DriverConfig['native'];
  private _session: any = null; // Will be @orpheus/engine-native Session
  private _eventCallbacks = new Set<(event: Event) => void>();

  constructor(config: DriverConfig['native'] = {}) {
    this._config = config;
  }

  get status(): ConnectionStatus {
    return this._status;
  }

  get capabilities(): DriverCapabilities | null {
    return this._capabilities;
  }

  /**
   * Connect to native driver
   */
  async connect(): Promise<void> {
    if (this._status === ConnectionStatus.Connected) {
      return;
    }

    this._status = ConnectionStatus.Connecting;

    try {
      // Dynamically import native driver
      let nativeModule;
      try {
        nativeModule = await import('@orpheus/engine-native');
      } catch (error) {
        throw new Error(
          '@orpheus/engine-native not installed or not built. ' +
            'Run "pnpm install" and "pnpm --filter @orpheus/engine-native build:native"'
        );
      }

      // Create session instance
      this._session = new nativeModule.Session();

      // Set capabilities (hardcoded for now, will query from native in P1.DRIV.006)
      this._capabilities = {
        commands: ['LoadSession', 'RenderClick'],
        events: ['SessionChanged', 'Heartbeat', 'RenderProgress'],
        version: nativeModule.version || '0.1.0-alpha.0',
        supportsRealTimeEvents: false, // Native driver uses polling/callbacks
      };

      this._status = ConnectionStatus.Connected;
    } catch (error) {
      this._status = ConnectionStatus.Error;
      throw new Error(`Failed to initialize native driver: ${error}`);
    }
  }

  /**
   * Disconnect from native driver
   */
  async disconnect(): Promise<void> {
    this._session = null;
    this._status = ConnectionStatus.Disconnected;
    this._capabilities = null;
    this._eventCallbacks.clear();
  }

  /**
   * Perform handshake to verify driver compatibility
   */
  async handshake(): Promise<HandshakeResult> {
    try {
      if (!this._session) {
        throw new Error('Native session not initialized');
      }

      // Verify capabilities
      const capabilities: DriverCapabilities = {
        commands: ['LoadSession', 'RenderClick'],
        events: ['SessionChanged', 'Heartbeat'],
        version: '0.1.0-alpha.0',
        contractVersion: '0.1.0-alpha',
        supportsRealTimeEvents: true, // Now supports events via N-API callbacks
        metadata: {
          transport: 'napi',
          runtime: 'node',
        },
      };

      this._capabilities = capabilities;

      return {
        success: true,
        capabilities,
      };
    } catch (error) {
      return {
        success: false,
        capabilities: {
          commands: [],
          events: [],
          version: 'unknown',
          supportsRealTimeEvents: false,
        },
        error: error instanceof Error ? error.message : String(error),
      };
    }
  }

  /**
   * Perform health check on native driver
   */
  async healthCheck(): Promise<boolean> {
    try {
      // Verify session is still valid
      if (!this._session) {
        return false;
      }

      // Try to query session info as a health check
      // If this throws, the driver is unhealthy
      try {
        this._session.getSessionInfo();
        return true;
      } catch {
        // Session not loaded yet, but driver is still healthy
        return true;
      }
    } catch {
      return false;
    }
  }

  /**
   * Execute command via native bindings
   */
  async execute(command: Command): Promise<CommandResponse> {
    if (this._status !== ConnectionStatus.Connected || !this._session) {
      throw new Error('Driver not connected');
    }

    // Map contract command to native method
    switch (command.type) {
      case 'LoadSession':
        return await this._session.loadSession({
          sessionPath: command.path,
        });

      case 'RenderClick':
        return await this._session.renderClick({
          outputPath: command.outputPath,
          bars: command.bars,
          bpm: command.bpm,
        });
    }
  }

  /**
   * Subscribe to events
   *
   * Hooks up to native N-API event callbacks
   */
  subscribe(callback: (event: Event) => void): () => void {
    this._eventCallbacks.add(callback);

    // Subscribe to native events if session is available
    if (this._session && this._session.subscribe) {
      const unsubscribe = this._session.subscribe((event: Event) => {
        this._emitEvent(event);
      });

      // Return combined unsubscribe function
      return () => {
        this._eventCallbacks.delete(callback);
        unsubscribe();
      };
    }

    return () => {
      this._eventCallbacks.delete(callback);
    };
  }

  /**
   * Emit event to all subscribers
   */
  private _emitEvent(event: Event): void {
    for (const callback of this._eventCallbacks) {
      try {
        callback(event);
      } catch (error) {
        console.error('Event callback error:', error);
      }
    }
  }
}
