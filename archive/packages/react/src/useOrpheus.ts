'use client';

/**
 * useOrpheus Hook
 *
 * Access OrpheusClient from React Context
 */

import { useContext } from 'react';
import { OrpheusContext } from './OrpheusContext.js';

/**
 * Access OrpheusClient from Context
 *
 * @throws {Error} If used outside of OrpheusProvider
 *
 * @example
 * ```tsx
 * function MyComponent() {
 *   const { client } = useOrpheus();
 *
 *   const handleLoad = async () => {
 *     await client.execute({
 *       type: 'LoadSession',
 *       path: './session.json',
 *     });
 *   };
 *
 *   return <button onClick={handleLoad}>Load Session</button>;
 * }
 * ```
 */
export function useOrpheus() {
  const context = useContext(OrpheusContext);

  if (!context) {
    throw new Error('useOrpheus must be used within OrpheusProvider');
  }

  return context;
}
