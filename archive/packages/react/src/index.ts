/**
 * @orpheus/react
 *
 * React integration for Orpheus SDK
 *
 * Provides Context provider and hooks for easy integration with React applications.
 */

// Provider and Context
export { OrpheusProvider, type OrpheusProviderProps } from './OrpheusProvider.js';
export { OrpheusContext, type OrpheusContextValue } from './OrpheusContext.js';

// Hooks
export { useOrpheus } from './useOrpheus.js';
export {
  useOrpheusCommand,
  type UseOrpheusCommandResult,
} from './useOrpheusCommand.js';
export {
  useOrpheusEvents,
  type UseOrpheusEventsOptions,
  type UseOrpheusEventsResult,
} from './useOrpheusEvents.js';

// Re-export client types for convenience
export type {
  OrpheusClient,
  DriverType,
  ConnectionStatus,
  ClientConfig,
  Command,
  Event,
  LoadSessionCommand,
  RenderClickCommand,
  SessionChangedEvent,
  HeartbeatEvent,
} from '@orpheus/client';
