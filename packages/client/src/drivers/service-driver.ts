/**
 * Service Driver Implementation
 *
 * Connects to @orpheus/engine-service via HTTP/WebSocket
 */

import type { Command, Event, ContractManifest } from '@orpheus/contract';
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
 * Service Driver
 *
 * Communicates with orpheusd service via HTTP/WebSocket
 */
export class ServiceDriver implements IDriver {
  readonly type = DriverType.Service;

  private _status: ConnectionStatus = ConnectionStatus.Disconnected;
  private _capabilities: DriverCapabilities | null = null;
  private _config: {
    url: string;
    authToken?: string;
    timeout: number;
  };
  private _ws: any | null = null; // WebSocket (browser/Node.js compatible)
  private _eventCallbacks = new Set<(event: Event) => void>();

  constructor(config: DriverConfig['service'] = {}) {
    this._config = {
      url: config.url || 'http://127.0.0.1:8080',
      authToken: config.authToken,
      timeout: config.timeout || 30000,
    };
  }

  get status(): ConnectionStatus {
    return this._status;
  }

  get capabilities(): DriverCapabilities | null {
    return this._capabilities;
  }

  /**
   * Connect to service and perform handshake
   */
  async connect(): Promise<void> {
    if (this._status === ConnectionStatus.Connected) {
      return;
    }

    this._status = ConnectionStatus.Connecting;

    try {
      // Fetch contract from service
      const contractInfo = await this._fetchContract();
      this._capabilities = {
        commands: contractInfo.commands || [],
        events: contractInfo.events || [],
        version: contractInfo.version || '0.1.0-alpha.0',
        supportsRealTimeEvents: true,
      };

      // Connect WebSocket for events
      await this._connectWebSocket();

      this._status = ConnectionStatus.Connected;
    } catch (error) {
      this._status = ConnectionStatus.Error;
      throw new Error(
        `Failed to connect to service at ${this._config.url}: ${
          error instanceof Error ? error.message : String(error)
        }`
      );
    }
  }

  /**
   * Disconnect from service
   */
  async disconnect(): Promise<void> {
    if (this._ws) {
      this._ws.close();
      this._ws = null;
    }

    this._status = ConnectionStatus.Disconnected;
    this._capabilities = null;
    this._eventCallbacks.clear();
  }

  /**
   * Perform handshake to verify driver compatibility
   */
  async handshake(): Promise<HandshakeResult> {
    try {
      // Fetch contract again to verify current capabilities
      const contractInfo = await this._fetchContract();

      const capabilities: DriverCapabilities = {
        commands: contractInfo.commands || [],
        events: contractInfo.events || [],
        version: contractInfo.version || '0.1.0-alpha.0',
        contractVersion: contractInfo.version,
        supportsRealTimeEvents: true,
        metadata: {
          transport: 'http+ws',
          url: this._config.url,
        },
      };

      // Update stored capabilities
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
   * Perform health check on service
   */
  async healthCheck(): Promise<boolean> {
    try {
      // Try to fetch /health endpoint
      const healthUrl = `${this._config.url}/health`;
      const headers: Record<string, string> = {};

      // Note: /health is typically public, no auth required

      const controller = new AbortController();
      const timeout = setTimeout(() => controller.abort(), 5000); // 5 second timeout for health checks

      try {
        const response = await fetch(healthUrl, {
          method: 'GET',
          headers,
          signal: controller.signal,
        });

        clearTimeout(timeout);

        // Also check WebSocket connection
        const wsHealthy = this._ws && this._ws.readyState === 1; // OPEN = 1

        return response.ok && wsHealthy;
      } catch {
        clearTimeout(timeout);
        return false;
      }
    } catch {
      return false;
    }
  }

  /**
   * Execute command via HTTP
   */
  async execute(command: Command): Promise<CommandResponse> {
    if (this._status !== ConnectionStatus.Connected) {
      throw new Error('Driver not connected');
    }

    const url = `${this._config.url}/command`;
    const headers: Record<string, string> = {
      'Content-Type': 'application/json',
    };

    if (this._config.authToken) {
      headers['Authorization'] = `Bearer ${this._config.authToken}`;
    }

    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), this._config.timeout);

    try {
      const response = await fetch(url, {
        method: 'POST',
        headers,
        body: JSON.stringify(command),
        signal: controller.signal,
      });

      if (!response.ok) {
        const error: any = await response.json().catch(() => ({
          error: { code: 'UNKNOWN', message: response.statusText },
        }));
        throw new Error(
          `Command failed: ${error.error?.message || response.statusText}`
        );
      }

      return (await response.json()) as CommandResponse;
    } finally {
      clearTimeout(timeout);
    }
  }

  /**
   * Subscribe to events
   */
  subscribe(callback: (event: Event) => void): () => void {
    this._eventCallbacks.add(callback);

    return () => {
      this._eventCallbacks.delete(callback);
    };
  }

  /**
   * Fetch contract manifest
   */
  private async _fetchContract(): Promise<{
    version: string;
    commands: string[];
    events: string[];
  }> {
    const url = `${this._config.url}/contract`;
    const headers: Record<string, string> = {};

    if (this._config.authToken) {
      headers['Authorization'] = `Bearer ${this._config.authToken}`;
    }

    const response = await fetch(url, { headers });

    if (!response.ok) {
      throw new Error(`Failed to fetch contract: ${response.statusText}`);
    }

    return (await response.json()) as { version: string; commands: string[]; events: string[] };
  }

  /**
   * Connect WebSocket for event streaming
   */
  private async _connectWebSocket(): Promise<void> {
    return new Promise((resolve, reject) => {
      // Convert HTTP URL to WebSocket URL
      const wsUrl = this._config.url.replace(/^http/, 'ws') + '/ws';

      // Note: WebSocket doesn't support custom headers in browser
      // Auth must be done via query param or during HTTP upgrade
      // For now, we rely on same-origin policy for security

      this._ws = new WebSocket(wsUrl);

      this._ws.onopen = () => {
        resolve();
      };

      this._ws.onerror = (_error: any) => {
        reject(new Error('WebSocket connection failed'));
      };

      this._ws.onmessage = (event: any) => {
        try {
          const message = JSON.parse(event.data);
          if (message.type === 'event' && message.payload) {
            this._emitEvent(message.payload);
          }
        } catch (error) {
          console.error('Failed to parse WebSocket message:', error);
        }
      };

      this._ws.onclose = () => {
        if (this._status === ConnectionStatus.Connected) {
          this._status = ConnectionStatus.Disconnected;
        }
      };
    });
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
