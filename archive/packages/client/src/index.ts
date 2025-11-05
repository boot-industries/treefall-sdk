/**
 * @orpheus/client
 *
 * Unified client broker for Orpheus SDK
 *
 * Provides automatic driver selection, connection management,
 * and a unified interface for command execution and event subscription.
 */

export { OrpheusClient } from './client.js';

export {
  DriverType,
  ConnectionStatus,
  type IDriver,
  type DriverCapabilities,
  type DriverConfig,
  type ClientConfig,
  type ClientEvent,
  type SubscriptionOptions,
  type CommandResponse,
  type HandshakeResult,
} from './types.js';

export { ServiceDriver } from './drivers/service-driver.js';
export { NativeDriver } from './drivers/native-driver.js';
export { WASMDriver } from './drivers/wasm-driver.js';

// Re-export contract types for convenience
export type {
  Command,
  Event,
  LoadSessionCommand,
  RenderClickCommand,
  SessionChangedEvent,
  HeartbeatEvent,
} from '@orpheus/contract';
