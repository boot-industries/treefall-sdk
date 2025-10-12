/**
 * Orpheus Client
 *
 * Unified client with automatic driver selection and handshake
 */

import type { Command, Event } from '@orpheus/contract';
import {
  DriverType,
  ConnectionStatus,
  type IDriver,
  type DriverCapabilities,
  type ClientConfig,
  type ClientEvent,
  type SubscriptionOptions,
  type CommandResponse,
} from './types.js';
import { ServiceDriver } from './drivers/service-driver.js';
import { NativeDriver } from './drivers/native-driver.js';

/**
 * Orpheus Client
 *
 * Main entry point for applications to interact with Orpheus SDK
 *
 * Features:
 * - Automatic driver selection based on availability and preference
 * - Unified command execution interface
 * - Event subscription with filtering
 * - Connection management and failover
 */
interface InternalClientConfig {
  driverPreference: DriverType[];
  drivers: NonNullable<ClientConfig['drivers']>;
  autoConnect: boolean;
  requiredCommands?: string[];
  requiredEvents?: string[];
  enableHealthChecks: boolean;
  healthCheckInterval: number;
  enableReconnection: boolean;
  maxReconnectionAttempts: number;
  reconnectionDelay: number;
}

export class OrpheusClient {
  private _config: InternalClientConfig;
  private _driver: IDriver | null = null;
  private _eventCallbacks = new Set<(event: ClientEvent) => void>();
  private _orpheusEventCallbacks = new Set<(event: Event) => void>();
  private _healthCheckInterval: NodeJS.Timeout | null = null;
  private _reconnectionAttempts = 0;
  private _isReconnecting = false;

  constructor(config: ClientConfig = {}) {
    this._config = {
      driverPreference: config.driverPreference || [
        DriverType.Native,
        DriverType.Service,
      ],
      drivers: config.drivers || {},
      autoConnect: config.autoConnect || false,
      requiredCommands: config.requiredCommands || undefined,
      requiredEvents: config.requiredEvents || undefined,
      enableHealthChecks: config.enableHealthChecks ?? true,
      healthCheckInterval: config.healthCheckInterval || 30000,
      enableReconnection: config.enableReconnection ?? true,
      maxReconnectionAttempts: config.maxReconnectionAttempts || 3,
      reconnectionDelay: config.reconnectionDelay || 1000,
    };

    if (this._config.autoConnect) {
      this.connect().catch((error) => {
        this._emitClientEvent({ type: 'error', error });
      });
    }
  }

  /**
   * Current driver in use
   */
  get driver(): IDriver | null {
    return this._driver;
  }

  /**
   * Connection status
   */
  get status(): ConnectionStatus {
    return this._driver?.status || ConnectionStatus.Disconnected;
  }

  /**
   * Driver capabilities
   */
  get capabilities(): DriverCapabilities | null {
    return this._driver?.capabilities || null;
  }

  /**
   * Connect to a driver
   *
   * Attempts to connect to drivers in preference order until one succeeds
   * Performs handshake and capability verification for each driver
   */
  async connect(driverType?: DriverType): Promise<void> {
    // If already connected, disconnect first
    if (this._driver) {
      await this.disconnect();
    }

    const driversToTry = driverType
      ? [driverType]
      : this._config.driverPreference;

    let lastError: Error | null = null;
    const attemptedDrivers: string[] = [];

    for (const type of driversToTry) {
      attemptedDrivers.push(type);

      try {
        const driver = this._createDriver(type);

        // Attempt connection
        await driver.connect();

        // Perform handshake to verify compatibility
        const handshakeResult = await driver.handshake();

        if (!handshakeResult.success) {
          throw new Error(
            `Handshake failed: ${handshakeResult.error || 'Unknown error'}`
          );
        }

        // Verify required capabilities
        if (!this._verifyCapabilities(handshakeResult.capabilities)) {
          throw new Error('Driver does not support required capabilities');
        }

        // Set up event forwarding
        driver.subscribe((event) => {
          this._emitOrpheusEvent(event);
        });

        this._driver = driver;
        this._reconnectionAttempts = 0;

        // Start health monitoring
        if (this._config.enableHealthChecks) {
          this._startHealthMonitoring();
        }

        this._emitClientEvent({
          type: 'connected',
          driver: type,
        });

        return;
      } catch (error) {
        lastError = error as Error;
        continue;
      }
    }

    throw new Error(
      `Failed to connect to any driver. Attempted: ${attemptedDrivers.join(', ')}. Last error: ${lastError?.message || 'Unknown error'}`
    );
  }

  /**
   * Disconnect from current driver
   */
  async disconnect(): Promise<void> {
    if (!this._driver) {
      return;
    }

    // Stop health monitoring
    this._stopHealthMonitoring();

    await this._driver.disconnect();
    this._driver = null;

    this._emitClientEvent({ type: 'disconnected' });
  }

  /**
   * Execute a command
   *
   * @throws {Error} If not connected or command fails
   */
  async execute(command: Command): Promise<CommandResponse> {
    if (!this._driver) {
      throw new Error('Not connected to any driver');
    }

    if (this._driver.status !== ConnectionStatus.Connected) {
      throw new Error(`Driver not ready: ${this._driver.status}`);
    }

    return await this._driver.execute(command);
  }

  /**
   * Subscribe to client events
   *
   * Client events include connection status changes and errors
   */
  on(callback: (event: ClientEvent) => void): () => void {
    this._eventCallbacks.add(callback);

    return () => {
      this._eventCallbacks.delete(callback);
    };
  }

  /**
   * Subscribe to Orpheus events
   *
   * Orpheus events are emitted by the SDK (SessionChanged, Heartbeat, etc.)
   */
  subscribe(
    callback: (event: Event) => void,
    options?: SubscriptionOptions
  ): () => void {
    const wrappedCallback = (event: Event) => {
      // Filter by event type if specified
      if (options?.eventTypes && !options.eventTypes.includes(event.type)) {
        return;
      }

      callback(event);
    };

    this._orpheusEventCallbacks.add(wrappedCallback);

    return () => {
      this._orpheusEventCallbacks.delete(wrappedCallback);
    };
  }

  /**
   * Create a driver instance
   */
  private _createDriver(type: DriverType): IDriver {
    switch (type) {
      case DriverType.Native:
        return new NativeDriver(this._config.drivers.native);

      case DriverType.Service:
        return new ServiceDriver(this._config.drivers.service);

      default:
        throw new Error(`Unknown driver type: ${type}`);
    }
  }

  /**
   * Emit a client event
   */
  private _emitClientEvent(event: ClientEvent): void {
    for (const callback of this._eventCallbacks) {
      try {
        callback(event);
      } catch (error) {
        console.error('Client event callback error:', error);
      }
    }
  }

  /**
   * Emit an Orpheus event
   */
  private _emitOrpheusEvent(event: Event): void {
    for (const callback of this._orpheusEventCallbacks) {
      try {
        callback(event);
      } catch (error) {
        console.error('Orpheus event callback error:', error);
      }
    }
  }

  /**
   * Verify that driver capabilities meet requirements
   */
  private _verifyCapabilities(capabilities: DriverCapabilities): boolean {
    // Check required commands
    if (this._config.requiredCommands) {
      for (const cmd of this._config.requiredCommands) {
        if (!capabilities.commands.includes(cmd)) {
          console.warn(`Driver missing required command: ${cmd}`);
          return false;
        }
      }
    }

    // Check required events
    if (this._config.requiredEvents) {
      for (const evt of this._config.requiredEvents) {
        if (!capabilities.events.includes(evt)) {
          console.warn(`Driver missing required event: ${evt}`);
          return false;
        }
      }
    }

    return true;
  }

  /**
   * Start health monitoring for current driver
   */
  private _startHealthMonitoring(): void {
    this._stopHealthMonitoring();

    this._healthCheckInterval = setInterval(async () => {
      if (!this._driver) {
        this._stopHealthMonitoring();
        return;
      }

      try {
        const isHealthy = await this._driver.healthCheck();

        if (!isHealthy) {
          console.warn('Driver health check failed');

          // Attempt reconnection if enabled
          if (this._config.enableReconnection && !this._isReconnecting) {
            this._attemptReconnection();
          }
        }
      } catch (error) {
        console.error('Health check error:', error);

        // Attempt reconnection on health check error
        if (this._config.enableReconnection && !this._isReconnecting) {
          this._attemptReconnection();
        }
      }
    }, this._config.healthCheckInterval);
  }

  /**
   * Stop health monitoring
   */
  private _stopHealthMonitoring(): void {
    if (this._healthCheckInterval) {
      clearInterval(this._healthCheckInterval);
      this._healthCheckInterval = null;
    }
  }

  /**
   * Attempt to reconnect to a driver
   */
  private async _attemptReconnection(): Promise<void> {
    if (this._isReconnecting) {
      return;
    }

    if (this._reconnectionAttempts >= this._config.maxReconnectionAttempts!) {
      console.error('Max reconnection attempts reached');
      this._emitClientEvent({
        type: 'error',
        error: new Error('Failed to reconnect after maximum attempts'),
      });
      return;
    }

    this._isReconnecting = true;
    this._reconnectionAttempts++;

    console.log(
      `Attempting reconnection (${this._reconnectionAttempts}/${this._config.maxReconnectionAttempts})`
    );

    // Wait before reconnecting
    await new Promise((resolve) =>
      setTimeout(resolve, this._config.reconnectionDelay)
    );

    try {
      await this.connect();
      this._isReconnecting = false;
      console.log('Reconnection successful');
    } catch (error) {
      this._isReconnecting = false;
      console.error('Reconnection failed:', error);

      // Try again if attempts remain
      if (this._reconnectionAttempts < this._config.maxReconnectionAttempts!) {
        this._attemptReconnection();
      } else {
        this._emitClientEvent({
          type: 'error',
          error: error as Error,
        });
      }
    }
  }
}
