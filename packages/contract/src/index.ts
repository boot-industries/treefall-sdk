/**
 * @orpheus/contract
 *
 * Orpheus SDK contract schemas and TypeScript types for command/event communication.
 * This package defines the API boundary between drivers and client applications.
 */

// ============================================================================
// Command Types
// ============================================================================

export interface LoadSessionCommand {
  type: 'LoadSession';
  path: string;
  id?: string;
}

export interface RenderClickCommand {
  type: 'RenderClick';
  outputPath: string;
  bars?: number;
  bpm?: number;
  id?: string;
}

export type Command =
  | LoadSessionCommand
  | RenderClickCommand;

// ============================================================================
// Event Types
// ============================================================================

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

export type Event =
  | SessionChangedEvent
  | HeartbeatEvent;

// ============================================================================
// Manifest and Versioning
// ============================================================================

export interface ContractVersion {
  version: string;
  path: string;
  checksum: string;
  status: 'alpha' | 'beta' | 'stable' | 'deprecated' | 'superseded';
}

export interface ContractManifest {
  currentVersion: string;
  versions: ContractVersion[];
}

// Import manifest (will be generated)
import manifestData from '../MANIFEST.json' assert { type: 'json' };
export const MANIFEST: ContractManifest = manifestData as ContractManifest;

// ============================================================================
// Validation (Placeholder - will be implemented with AJV)
// ============================================================================

export interface ValidationResult {
  valid: boolean;
  errors?: string[];
}

export function validateCommand(command: unknown): ValidationResult {
  // TODO: Implement AJV validation in TASK-016
  return { valid: true };
}

export function validateEvent(event: unknown): ValidationResult {
  // TODO: Implement AJV validation in TASK-016
  return { valid: true };
}
