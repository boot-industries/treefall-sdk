/**
 * Orpheus Native Driver
 *
 * Direct N-API bindings to Orpheus C++ SDK for maximum performance
 */

import { join } from 'path';

// Load the native addon
// The .node file will be in the build directory created by cmake-js
let nativeBinding: NativeBindings;

try {
  // Try to load from build directory
  nativeBinding = require(join(__dirname, '../build/Release/orpheus_native.node'));
} catch (e) {
  try {
    // Fallback to Debug build
    nativeBinding = require(join(__dirname, '../build/Debug/orpheus_native.node'));
  } catch (e2) {
    throw new Error(
      'Failed to load Orpheus native bindings. Run "pnpm build:native" first.\n' +
      `Tried:\n  - build/Release/orpheus_native.node\n  - build/Debug/orpheus_native.node`
    );
  }
}

/**
 * Session wrapper class
 */
export class Session {
  private _native: NativeSession;

  constructor() {
    this._native = new nativeBinding.Session();
  }

  /**
   * Load a session from JSON file
   */
  async loadSession(payload: { sessionPath: string }): Promise<LoadSessionResult> {
    return this._native.loadSession(payload);
  }

  /**
   * Get session information
   */
  getSessionInfo(): SessionInfo {
    return this._native.getSessionInfo();
  }

  /**
   * Render click track
   */
  async renderClick(payload: RenderClickPayload): Promise<RenderClickResult> {
    return this._native.renderClick(payload);
  }

  /**
   * Get current tempo
   */
  getTempo(): number {
    return this._native.getTempo();
  }

  /**
   * Set tempo
   */
  setTempo(tempo: number): void {
    this._native.setTempo(tempo);
  }

  /**
   * Subscribe to events
   * @returns Unsubscribe function
   */
  subscribe(callback: (event: OrpheusEvent) => void): () => void {
    return this._native.subscribe(callback);
  }
}

/**
 * Module metadata
 */
export const version = nativeBinding.version as string;
export const driver = nativeBinding.driver as string;

/**
 * Type definitions
 */

interface NativeBindings {
  Session: new () => NativeSession;
  version: string;
  driver: string;
}

interface NativeSession {
  loadSession(payload: { sessionPath: string }): LoadSessionResult;
  getSessionInfo(): SessionInfo;
  renderClick(payload: RenderClickPayload): RenderClickResult;
  getTempo(): number;
  setTempo(tempo: number): void;
  subscribe(callback: (event: OrpheusEvent) => void): () => void;
}

export interface LoadSessionResult {
  success: boolean;
  message?: string;
  result?: {
    sessionPath: string;
    sessionName?: string;
    trackCount?: number;
    tempo?: number;
  };
}

export interface SessionInfo {
  name: string;
  tempo: number;
  trackCount: number;
}

export interface RenderClickPayload {
  outputPath?: string;
  tempo?: number;
  bars?: number;
  sampleRate?: number;
}

export interface RenderClickResult {
  success: boolean;
  message?: string;
  outputPath?: string;
}

/**
 * Orpheus event types
 */
export type OrpheusEvent = SessionChangedEvent | HeartbeatEvent;

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
