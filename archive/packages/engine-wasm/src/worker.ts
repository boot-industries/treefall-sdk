/**
 * @file worker.ts
 * @brief Web Worker wrapper for Orpheus WASM module
 *
 * Runs WASM module in dedicated worker thread to prevent blocking main thread
 * during audio processing operations.
 */

import { loadWASM, isWASMSupported } from './loader.js';
import type { OrpheusModule, IntegrityManifest } from './index.js';

/**
 * Worker message types for command/event communication
 */
export interface WorkerCommand {
  id: string;
  type: string;
  data?: unknown;
}

export interface WorkerResponse {
  id: string;
  type: 'success' | 'error' | 'event';
  data?: unknown;
  error?: string;
}

/**
 * Worker state
 */
let orpheusModule: OrpheusModule | null = null;
let isInitialized = false;

/**
 * Handle incoming messages from main thread
 */
self.onmessage = async (event: MessageEvent<WorkerCommand>) => {
  const { id, type, data } = event.data;

  try {
    switch (type) {
      case 'init':
        await handleInit(id, data as { baseUrl: string; integrity?: IntegrityManifest });
        break;

      case 'getVersion':
        handleGetVersion(id);
        break;

      case 'loadSession':
        handleLoadSession(id, data as { jsonString: string });
        break;

      case 'renderClick':
        handleRenderClick(id, data as { bpm: number; bars: number });
        break;

      case 'getTempo':
        handleGetTempo(id);
        break;

      case 'setTempo':
        handleSetTempo(id, data as { bpm: number });
        break;

      case 'shutdown':
        handleShutdown(id);
        break;

      default:
        postError(id, `Unknown command type: ${type}`);
    }
  } catch (error) {
    postError(id, error instanceof Error ? error.message : String(error));
  }
};

/**
 * Initialize WASM module
 */
async function handleInit(
  id: string,
  config: { baseUrl: string; integrity?: IntegrityManifest }
): Promise<void> {
  if (!isWASMSupported()) {
    throw new Error('WebAssembly not supported in this environment');
  }

  if (isInitialized) {
    postSuccess(id, { message: 'Already initialized' });
    return;
  }

  // Load WASM module
  orpheusModule = await loadWASM(config.baseUrl, config.integrity);

  // Initialize SDK
  const success = orpheusModule.initialize();
  if (!success) {
    throw new Error('Failed to initialize Orpheus SDK');
  }

  isInitialized = true;

  postSuccess(id, {
    version: orpheusModule.getVersion(),
    message: 'Orpheus WASM initialized',
  });
}

/**
 * Get version command
 */
function handleGetVersion(id: string): void {
  if (!orpheusModule) {
    throw new Error('Module not initialized');
  }

  postSuccess(id, {
    version: orpheusModule.getVersion(),
  });
}

/**
 * Load session command
 */
function handleLoadSession(id: string, data: { jsonString: string }): void {
  if (!orpheusModule) {
    throw new Error('Module not initialized');
  }

  const success = orpheusModule.loadSession(data.jsonString);
  if (!success) {
    throw new Error('Failed to load session');
  }

  // Emit SessionChanged event
  emitEvent('SessionChanged', {
    tempo: orpheusModule.getTempo(),
  });

  postSuccess(id, { message: 'Session loaded successfully' });
}

/**
 * Render click command
 */
function handleRenderClick(id: string, data: { bpm: number; bars: number }): void {
  if (!orpheusModule) {
    throw new Error('Module not initialized');
  }

  const resultJSON = orpheusModule.renderClick(data.bpm, data.bars);
  const result = JSON.parse(resultJSON);

  if (result.error) {
    throw new Error(result.error);
  }

  postSuccess(id, result);
}

/**
 * Get tempo command
 */
function handleGetTempo(id: string): void {
  if (!orpheusModule) {
    throw new Error('Module not initialized');
  }

  postSuccess(id, {
    tempo: orpheusModule.getTempo(),
  });
}

/**
 * Set tempo command
 */
function handleSetTempo(id: string, data: { bpm: number }): void {
  if (!orpheusModule) {
    throw new Error('Module not initialized');
  }

  orpheusModule.setTempo(data.bpm);

  // Emit SessionChanged event
  emitEvent('SessionChanged', {
    tempo: data.bpm,
  });

  postSuccess(id, { message: 'Tempo updated' });
}

/**
 * Shutdown command
 */
function handleShutdown(id: string): void {
  if (orpheusModule) {
    orpheusModule.shutdown();
    orpheusModule = null;
    isInitialized = false;
  }

  postSuccess(id, { message: 'Shutdown complete' });
}

/**
 * Post success response
 */
function postSuccess(id: string, data: unknown): void {
  const response: WorkerResponse = {
    id,
    type: 'success',
    data,
  };
  self.postMessage(response);
}

/**
 * Post error response
 */
function postError(id: string, error: string): void {
  const response: WorkerResponse = {
    id,
    type: 'error',
    error,
  };
  self.postMessage(response);
}

/**
 * Emit event to main thread
 */
export function emitEvent(eventType: string, eventData: unknown): void {
  const response: WorkerResponse = {
    id: crypto.randomUUID(),
    type: 'event',
    data: {
      eventType,
      ...(eventData as Record<string, unknown>),
    },
  };
  self.postMessage(response);
}

// Log worker ready
console.log('[OrpheusWorker] Ready and waiting for init command');
