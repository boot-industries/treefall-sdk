/**
 * @file loader.ts
 * @brief Secure WASM module loader with SRI verification
 */

import type { OrpheusModule } from './types.js';

/**
 * Integrity metadata for WASM artifacts
 */
export interface IntegrityManifest {
  version: string;
  generated: string;
  emscripten: string;
  files: {
    'orpheus.wasm': string;
    'orpheus.js': string;
  };
}

/**
 * Load Orpheus WASM module with security guarantees
 *
 * @param baseUrl - Base URL where WASM artifacts are hosted (must be same-origin)
 * @param integrity - Optional integrity manifest for SRI verification
 * @returns Initialized Orpheus module
 *
 * @throws {Error} If SRI verification fails or MIME type incorrect
 *
 * @example
 * ```ts
 * import integrity from '../build/integrity.json';
 *
 * const module = await loadWASM('/assets/wasm/', integrity);
 * console.log(module.getVersion());
 * ```
 */
export async function loadWASM(
  baseUrl: string,
  integrity?: IntegrityManifest
): Promise<OrpheusModule> {
  // Ensure base URL ends with /
  const base = baseUrl.endsWith('/') ? baseUrl : `${baseUrl}/`;

  // Fetch WASM with SRI if available
  const wasmUrl = `${base}orpheus.wasm`;
  const fetchOptions: RequestInit = {
    mode: 'same-origin', // Prevent cross-origin attacks
  };

  if (integrity) {
    fetchOptions.integrity = integrity.files['orpheus.wasm'];
  }

  const response = await fetch(wasmUrl, fetchOptions);

  if (!response.ok) {
    throw new Error(`Failed to fetch WASM: ${response.statusText}`);
  }

  // Verify MIME type (security requirement)
  const contentType = response.headers.get('Content-Type');
  if (contentType !== 'application/wasm') {
    throw new Error(
      `Invalid WASM MIME type: expected 'application/wasm', got '${contentType}'`
    );
  }

  // Get WASM bytes for Emscripten factory
  const wasmBytes = await response.arrayBuffer();

  // Load JS glue code (dynamic import with SRI)
  const jsUrl = `${base}orpheus.js`;

  // Note: Dynamic import doesn't support integrity directly in most browsers yet
  // Production deployments should serve with CSP headers for additional security
  const glueModule = await import(/* @vite-ignore */ jsUrl);

  // Initialize the module
  const createModule = glueModule.default || glueModule.createOrpheusModule;

  if (!createModule) {
    throw new Error('Failed to load Orpheus module factory');
  }

  const orpheusModule: OrpheusModule = await createModule({
    wasmBinary: wasmBytes,
  });

  return orpheusModule;
}

/**
 * Check if WASM is supported in current environment
 */
export function isWASMSupported(): boolean {
  return (
    typeof WebAssembly !== 'undefined' &&
    typeof WebAssembly.instantiate === 'function'
  );
}
