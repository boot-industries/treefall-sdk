'use client';

/**
 * Orpheus React Context
 *
 * Provides OrpheusClient instance to React component tree
 */

import { createContext } from 'react';
import type { OrpheusClient } from '@orpheus/client';

/**
 * Orpheus Context
 *
 * Provides access to OrpheusClient instance throughout the React tree
 */
export interface OrpheusContextValue {
  /** Orpheus client instance */
  client: OrpheusClient;
}

/**
 * Orpheus React Context
 */
export const OrpheusContext = createContext<OrpheusContextValue | null>(null);

OrpheusContext.displayName = 'OrpheusContext';
