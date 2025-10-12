/**
 * Orpheus Client - Type Definitions
 */

import type { Command, Event } from '@orpheus/contract';

/**
 * Command response
 */
export interface CommandResponse {
  success: boolean;
  requestId?: string;
  result?: unknown;
  error?: {
    code: string;
    message: string;
    details?: unknown;
  };
}

/**
 * Driver types supported by the client broker
 */
export enum DriverType {
  /** Direct N-API bindings (highest performance) */
  Native = 'native',
  /** HTTP/WebSocket service (cross-process) */
  Service = 'service',
}

/**
 * Driver connection status
 */
export enum ConnectionStatus {
  /** Not connected */
  Disconnected = 'disconnected',
  /** Connecting to driver */
  Connecting = 'connecting',
  /** Connected and ready */
  Connected = 'connected',
  /** Connection error */
  Error = 'error',
}

/**
 * Driver capabilities
 */
export interface DriverCapabilities {
  /** Commands supported by this driver */
  commands: string[];
  /** Events emitted by this driver */
  events: string[];
  /** Driver version */
  version: string;
  /** Contract version supported */
  contractVersion?: string;
  /** Whether real-time event streaming is supported */
  supportsRealTimeEvents: boolean;
  /** Additional driver metadata */
  metadata?: Record<string, unknown>;
}

/**
 * Handshake result
 */
export interface HandshakeResult {
  /** Whether handshake was successful */
  success: boolean;
  /** Driver capabilities discovered during handshake */
  capabilities: DriverCapabilities;
  /** Error message if handshake failed */
  error?: string;
}

/**
 * Driver interface
 *
 * All drivers must implement this interface to be compatible with the client broker
 */
export interface IDriver {
  /** Driver type identifier */
  readonly type: DriverType;

  /** Current connection status */
  readonly status: ConnectionStatus;

  /** Driver capabilities (available after handshake) */
  readonly capabilities: DriverCapabilities | null;

  /**
   * Connect to the driver and perform handshake
   */
  connect(): Promise<void>;

  /**
   * Disconnect from the driver
   */
  disconnect(): Promise<void>;

  /**
   * Perform handshake to verify driver compatibility
   * @returns Handshake result with capabilities
   */
  handshake(): Promise<HandshakeResult>;

  /**
   * Perform health check on driver
   * @returns true if driver is healthy, false otherwise
   */
  healthCheck(): Promise<boolean>;

  /**
   * Execute a command
   */
  execute(command: Command): Promise<CommandResponse>;

  /**
   * Subscribe to events
   * @returns Unsubscribe function
   */
  subscribe(callback: (event: Event) => void): () => void;
}

/**
 * Driver configuration
 */
export interface DriverConfig {
  /** Driver type */
  type: DriverType;

  /** Service driver configuration */
  service?: {
    /** Service URL (default: http://127.0.0.1:8080) */
    url?: string;
    /** Authentication token */
    authToken?: string;
    /** Request timeout in milliseconds */
    timeout?: number;
  };

  /** Native driver configuration */
  native?: {
    /** Path to native addon (optional, auto-detected by default) */
    addonPath?: string;
  };
}

/**
 * Client configuration
 */
export interface ClientConfig {
  /**
   * Driver preference order
   *
   * The client will attempt to connect to drivers in this order.
   * If not specified, defaults to [Native, Service]
   */
  driverPreference?: DriverType[];

  /**
   * Driver-specific configurations
   */
  drivers?: {
    service?: DriverConfig['service'];
    native?: DriverConfig['native'];
  };

  /**
   * Auto-connect on instantiation
   * @default false
   */
  autoConnect?: boolean;

  /**
   * Required commands that driver must support
   * If specified, driver selection will skip drivers that don't support these commands
   */
  requiredCommands?: string[];

  /**
   * Required events that driver must support
   * If specified, driver selection will skip drivers that don't support these events
   */
  requiredEvents?: string[];

  /**
   * Enable health checks
   * @default true
   */
  enableHealthChecks?: boolean;

  /**
   * Health check interval in milliseconds
   * @default 30000 (30 seconds)
   */
  healthCheckInterval?: number;

  /**
   * Enable automatic reconnection on driver failure
   * @default true
   */
  enableReconnection?: boolean;

  /**
   * Maximum reconnection attempts
   * @default 3
   */
  maxReconnectionAttempts?: number;

  /**
   * Reconnection delay in milliseconds
   * @default 1000
   */
  reconnectionDelay?: number;
}

/**
 * Event subscription options
 */
export interface SubscriptionOptions {
  /** Event type filter (if specified, only these event types will be delivered) */
  eventTypes?: string[];
}

/**
 * Client events
 */
export type ClientEvent =
  | { type: 'connected'; driver: DriverType }
  | { type: 'disconnected' }
  | { type: 'error'; error: Error }
  | { type: 'driver-changed'; from: DriverType | null; to: DriverType };
