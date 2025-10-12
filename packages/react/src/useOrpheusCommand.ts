'use client';

/**
 * useOrpheusCommand Hook
 *
 * Execute Orpheus commands with React state management
 */

import { useState, useCallback } from 'react';
import type { Command, CommandResponse } from '@orpheus/client';
import { useOrpheus } from './useOrpheus.js';

export interface UseOrpheusCommandResult {
  /** Execute a command */
  execute: (command: Command) => Promise<CommandResponse>;

  /** Loading state */
  loading: boolean;

  /** Error state */
  error: Error | null;

  /** Last response */
  data: CommandResponse | null;

  /** Reset state */
  reset: () => void;
}

/**
 * Execute Orpheus commands with React state management
 *
 * @example
 * ```tsx
 * function LoadSessionButton() {
 *   const { execute, loading, error, data } = useOrpheusCommand();
 *
 *   const handleClick = () => {
 *     execute({
 *       type: 'LoadSession',
 *       path: './session.json',
 *     });
 *   };
 *
 *   return (
 *     <div>
 *       <button onClick={handleClick} disabled={loading}>
 *         {loading ? 'Loading...' : 'Load Session'}
 *       </button>
 *       {error && <p>Error: {error.message}</p>}
 *       {data && <p>Success!</p>}
 *     </div>
 *   );
 * }
 * ```
 */
export function useOrpheusCommand(): UseOrpheusCommandResult {
  const { client } = useOrpheus();

  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<Error | null>(null);
  const [data, setData] = useState<CommandResponse | null>(null);

  const execute = useCallback(
    async (command: Command): Promise<CommandResponse> => {
      setLoading(true);
      setError(null);

      try {
        const response = await client.execute(command);
        setData(response);
        return response;
      } catch (err) {
        const error = err instanceof Error ? err : new Error(String(err));
        setError(error);
        throw error;
      } finally {
        setLoading(false);
      }
    },
    [client]
  );

  const reset = useCallback(() => {
    setLoading(false);
    setError(null);
    setData(null);
  }, []);

  return {
    execute,
    loading,
    error,
    data,
    reset,
  };
}
