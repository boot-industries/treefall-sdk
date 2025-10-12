/**
 * Orpheus Minhost Executor - Bridge to C++ SDK via child process
 */

import { spawn } from 'child_process';
import { resolve as pathResolve } from 'path';
import type { CommandRequest, CommandResponse } from '../types.js';

/**
 * Find the orpheus_minhost binary
 */
function findMinhostBinary(): string {
  // Check environment variable first
  if (process.env.ORPHEUS_MINHOST_PATH) {
    return process.env.ORPHEUS_MINHOST_PATH;
  }

  // Get SDK root - service runs from SDK root directory
  const sdkRoot = process.env.ORPHEUS_SDK_ROOT || process.cwd();

  // Try multiple potential locations
  const candidates = [
    // Release build (preferred - no sanitizers)
    pathResolve(sdkRoot, 'build-release/adapters/minhost/orpheus_minhost'),
    // Debug build
    pathResolve(sdkRoot, 'build/adapters/minhost/orpheus_minhost'),
    // CI/packaged location
    pathResolve(sdkRoot, 'bin/orpheus_minhost'),
  ];

  // For now, use the first candidate
  // TODO: Add actual file existence checks with fs.existsSync
  return candidates[0];
}

export interface MinhostExecutorOptions {
  minhostPath?: string;
  timeout?: number;
}

/**
 * Execute a command via orpheus_minhost CLI
 */
export async function executeMinhostCommand(
  command: string,
  args: string[],
  options: MinhostExecutorOptions = {}
): Promise<unknown> {
  const minhostPath = options.minhostPath || findMinhostBinary();
  const timeout = options.timeout || 30000; // 30 seconds default

  // Get SDK root for working directory
  const sdkRoot = process.env.ORPHEUS_SDK_ROOT || process.cwd();

  return new Promise((resolve, reject) => {
    const fullArgs = ['--json', command, ...args];

    // Set up library path for dylib resolution
    // On macOS, minhost needs to find liborpheus_*.dylib files
    const env = { ...process.env };
    const buildType = minhostPath.includes('build-release') ? 'build-release' : 'build';
    const libPath = pathResolve(sdkRoot, `${buildType}/src`);

    // Add dylib path to environment
    if (process.platform === 'darwin') {
      env.DYLD_LIBRARY_PATH = libPath + (env.DYLD_LIBRARY_PATH ? `:${env.DYLD_LIBRARY_PATH}` : '');
    } else if (process.platform === 'linux') {
      env.LD_LIBRARY_PATH = libPath + (env.LD_LIBRARY_PATH ? `:${env.LD_LIBRARY_PATH}` : '');
    }

    const proc = spawn(minhostPath, fullArgs, {
      cwd: sdkRoot, // Run from SDK root so relative paths work
      stdio: ['ignore', 'pipe', 'pipe'],
      env, // Pass environment with library paths
    });

    let stdout = '';
    let stderr = '';
    let timedOut = false;

    const timer = setTimeout(() => {
      timedOut = true;
      proc.kill('SIGTERM');
      reject(new Error(`Minhost command timed out after ${timeout}ms`));
    }, timeout);

    proc.stdout?.on('data', (data: Buffer) => {
      stdout += data.toString();
    });

    proc.stderr?.on('data', (data: Buffer) => {
      stderr += data.toString();
    });

    proc.on('error', (error: Error) => {
      clearTimeout(timer);
      reject(new Error(`Failed to spawn minhost at ${minhostPath}: ${error.message}`));
    });

    proc.on('close', (code: number | null) => {
      clearTimeout(timer);

      if (timedOut) {
        return; // Already rejected
      }

      // code === null means signal exit (can happen on successful exit too)
      // code === 0 means normal successful exit
      if (code !== null && code !== 0) {
        // Try to parse error from stderr or stdout
        let errorMessage = `Minhost exited with code ${code}`;

        // Include stderr for debugging
        if (stderr) {
          errorMessage += `\nStderr: ${stderr.trim()}`;
        }
        if (stdout) {
          errorMessage += `\nStdout: ${stdout.trim()}`;
        }

        try {
          const errorJson = JSON.parse(stdout || stderr);
          if (errorJson.error) {
            errorMessage = errorJson.error.message || errorMessage;
          }
        } catch {
          // Not JSON, already appended raw output above
        }
        reject(new Error(errorMessage));
        return;
      }

      try {
        const result = JSON.parse(stdout);
        resolve(result);
      } catch (error) {
        reject(new Error(`Failed to parse minhost output: ${(error as Error).message}`));
      }
    });
  });
}

/**
 * Load a session via minhost
 */
export async function loadSession(sessionPath: string, options: MinhostExecutorOptions = {}): Promise<unknown> {
  const args = ['--session', sessionPath];
  return executeMinhostCommand('load', args, options);
}

/**
 * Render a click track via minhost
 */
export async function renderClick(
  sessionPath: string,
  outputPath: string,
  options: {
    sampleRate?: number;
    bars?: number;
    bpm?: number;
  } & MinhostExecutorOptions = {}
): Promise<unknown> {
  const args = ['--session', sessionPath, '--out', outputPath];

  if (options.sampleRate) {
    args.push('--sr', options.sampleRate.toString());
  }

  // TODO: Add support for bars and bpm overrides via spec file

  return executeMinhostCommand('render-click', args, options);
}

/**
 * Map contract command to minhost command
 */
export async function executeOrpheusCommand(
  request: CommandRequest,
  options: MinhostExecutorOptions = {}
): Promise<CommandResponse> {
  try {
    let result: unknown;

    switch (request.type) {
      case 'LoadSession': {
        const payload = request.payload as { sessionId?: string; sessionPath?: string };
        if (!payload.sessionPath) {
          return {
            success: false,
            requestId: request.requestId,
            error: {
              code: 'INVALID_PAYLOAD',
              message: 'LoadSession requires sessionPath',
            },
          };
        }
        result = await loadSession(payload.sessionPath, options);
        break;
      }

      case 'RenderClick': {
        const payload = request.payload as {
          sessionPath?: string;
          outputPath?: string;
          sampleRate?: number;
          bars?: number;
          bpm?: number;
        };
        if (!payload.sessionPath || !payload.outputPath) {
          return {
            success: false,
            requestId: request.requestId,
            error: {
              code: 'INVALID_PAYLOAD',
              message: 'RenderClick requires sessionPath and outputPath',
            },
          };
        }
        result = await renderClick(payload.sessionPath, payload.outputPath, {
          sampleRate: payload.sampleRate,
          bars: payload.bars,
          bpm: payload.bpm,
          ...options,
        });
        break;
      }

      default:
        return {
          success: false,
          requestId: request.requestId,
          error: {
            code: 'UNKNOWN_COMMAND',
            message: `Unknown command type: ${request.type}`,
          },
        };
    }

    return {
      success: true,
      requestId: request.requestId,
      result,
    };
  } catch (error) {
    return {
      success: false,
      requestId: request.requestId,
      error: {
        code: 'EXECUTION_FAILED',
        message: error instanceof Error ? error.message : 'Unknown error',
        details: error,
      },
    };
  }
}
