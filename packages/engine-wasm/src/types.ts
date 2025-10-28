/**
 * @file types.ts
 * @brief TypeScript type definitions for Orpheus WASM module
 */

/**
 * Orpheus WASM module interface
 *
 * These types match the Emscripten bindings in wasm_bindings.cpp
 */
export interface OrpheusModule {
  /**
   * Get SDK version
   * @returns Version string (semver format)
   */
  getVersion(): string;

  /**
   * Initialize Orpheus engine
   * @returns true if successful
   */
  initialize(): boolean;

  /**
   * Shutdown Orpheus engine
   */
  shutdown(): void;

  /**
   * Load session from JSON string
   * @param jsonString Session JSON
   * @returns true if successful
   */
  loadSession(jsonString: string): boolean;

  /**
   * Render click track
   * @param bpm Beats per minute
   * @param bars Number of bars
   * @returns JSON result string
   */
  renderClick(bpm: number, bars: number): string;

  /**
   * Get current session tempo
   * @returns BPM value
   */
  getTempo(): number;

  /**
   * Set session tempo
   * @param bpm Beats per minute
   */
  setTempo(bpm: number): void;
}

/**
 * Emscripten module factory options
 */
export interface EmscriptenModuleOptions {
  wasmBinary?: ArrayBuffer;
  locateFile?: (path: string) => string;
  print?: (text: string) => void;
  printErr?: (text: string) => void;
}
