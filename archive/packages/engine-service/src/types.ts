/**
 * Type definitions for Orpheus Service Driver
 */

export interface ServiceConfig {
  /** Port to listen on */
  port: number;
  /** Host to bind to (default: 127.0.0.1) */
  host: string;
  /** Log level */
  logLevel: 'trace' | 'debug' | 'info' | 'warn' | 'error';
  /** Optional authentication token */
  authToken?: string;
}

export interface HealthResponse {
  status: 'ok' | 'degraded' | 'error';
  uptime: number;
  version: string;
  timestamp: number;
}

export interface VersionResponse {
  service: string;
  serviceVersion: string;
  sdkVersion: string;
  contractVersion: string;
  nodeVersion: string;
}

export interface ContractResponse {
  version: string;
  commands: string[];
  events: string[];
  schemaUrl: string;
}

export interface CommandRequest {
  type: string;
  payload: unknown;
  requestId?: string;
}

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

export interface WebSocketMessage {
  type: 'event' | 'error' | 'ping' | 'pong';
  payload?: unknown;
  timestamp: number;
}
