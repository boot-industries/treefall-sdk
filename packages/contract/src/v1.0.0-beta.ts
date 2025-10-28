/**
 * @orpheus/contract v1.0.0-beta
 *
 * Contract v1.0.0-beta - Full command/event set for Phase 2
 */

// ============================================================================
// Command Types
// ============================================================================

export interface LoadSessionCommand {
  type: 'LoadSession';
  path: string;
  id?: string;
}

export interface SaveSessionCommand {
  type: 'SaveSession';
  path?: string;
  id?: string;
}

export interface TriggerClipGridSceneCommand {
  type: 'TriggerClipGridScene';
  sceneId: string;
  id?: string;
}

export interface RenderClickCommand {
  type: 'RenderClick';
  outputPath: string;
  bars?: number;
  bpm?: number;
  id?: string;
}

export interface SetTransportCommand {
  type: 'SetTransport';
  position: number;
  playing: boolean;
  id?: string;
}

export type Command =
  | LoadSessionCommand
  | SaveSessionCommand
  | TriggerClipGridSceneCommand
  | RenderClickCommand
  | SetTransportCommand;

// ============================================================================
// Event Types
// ============================================================================

export interface TransportTickEvent {
  type: 'TransportTick';
  timestamp: number;
  position: number;
  beat?: number;
  bar?: number;
  tempo?: number;
  sequenceId?: number;
}

export interface RenderProgressEvent {
  type: 'RenderProgress';
  timestamp: number;
  percentage: number;
  renderId?: string;
  sequenceId?: number;
}

export interface RenderDoneEvent {
  type: 'RenderDone';
  timestamp: number;
  renderId?: string;
  outputPath?: string;
  sampleRate?: number;
  channels?: number;
  duration?: number;
  sequenceId?: number;
}

export interface SessionChangedEvent {
  type: 'SessionChanged';
  timestamp: number;
  sessionPath?: string;
  trackCount?: number;
  sequenceId?: number;
}

export interface HeartbeatEvent {
  type: 'Heartbeat';
  timestamp: number;
  uptime?: number;
  sequenceId?: number;
}

export interface ErrorEvent {
  type: 'Error';
  timestamp: number;
  kind: 'Validation' | 'Render' | 'Transport' | 'System';
  code: string;
  message: string;
  details?: Record<string, unknown>;
  sequenceId?: number;
}

export type Event =
  | TransportTickEvent
  | RenderProgressEvent
  | RenderDoneEvent
  | SessionChangedEvent
  | HeartbeatEvent
  | ErrorEvent;

// ============================================================================
// Event Frequency Constraints (for validation)
// ============================================================================

export const EVENT_FREQUENCY_LIMITS = {
  TransportTick: 30, // Hz
  RenderProgress: 10, // Hz
  Heartbeat: 0.1, // Hz (every 10s)
} as const;

// ============================================================================
// Event Frequency Validation
// ============================================================================

export {
  EventFrequencyValidator,
  createThrottledEmitter,
  type FrequencyViolation,
} from './frequency-validator.js';
