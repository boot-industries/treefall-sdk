'use client';

/**
 * Orpheus Provider Component
 *
 * Wraps the application to provide OrpheusClient access via Context
 */

import React, { useEffect, useState, useRef } from 'react';
import { OrpheusClient, type ClientConfig, ConnectionStatus } from '@orpheus/client';
import { OrpheusContext } from './OrpheusContext.js';

export interface OrpheusProviderProps {
  /** OrpheusClient configuration */
  config?: ClientConfig;

  /** Pre-configured OrpheusClient instance (if provided, config is ignored) */
  client?: OrpheusClient;

  /** Children components */
  children: React.ReactNode;

  /** Callback when connection status changes */
  onStatusChange?: (status: ConnectionStatus) => void;

  /** Callback when connection errors occur */
  onError?: (error: Error) => void;
}

/**
 * Orpheus Provider
 *
 * Provides OrpheusClient instance to the React component tree.
 *
 * @example
 * ```tsx
 * <OrpheusProvider config={{ autoConnect: true }}>
 *   <App />
 * </OrpheusProvider>
 * ```
 */
export function OrpheusProvider({
  config,
  client: providedClient,
  children,
  onStatusChange,
  onError,
}: OrpheusProviderProps) {
  // Use provided client or create one from config
  const clientRef = useRef<OrpheusClient | null>(
    providedClient || (config ? new OrpheusClient(config) : null)
  );

  const [isReady, setIsReady] = useState(false);

  useEffect(() => {
    const client = clientRef.current;

    if (!client) {
      console.error('OrpheusProvider: No client provided and no config specified');
      return;
    }

    // Subscribe to client events
    const unsubscribe = client.on((event) => {
      switch (event.type) {
        case 'connected':
          setIsReady(true);
          onStatusChange?.(ConnectionStatus.Connected);
          break;

        case 'disconnected':
          setIsReady(false);
          onStatusChange?.(ConnectionStatus.Disconnected);
          break;

        case 'error':
          onError?.(event.error);
          break;
      }
    });

    // Auto-connect if not configured to do so
    if (!config?.autoConnect && client.status === ConnectionStatus.Disconnected) {
      client.connect().catch((error) => {
        console.error('OrpheusProvider: Failed to connect:', error);
        onError?.(error);
      });
    }

    // Check if already connected
    if (client.status === ConnectionStatus.Connected) {
      setIsReady(true);
    }

    // Cleanup on unmount
    return () => {
      unsubscribe();
      client.disconnect().catch((error) => {
        console.error('OrpheusProvider: Failed to disconnect:', error);
      });
    };
  }, [config?.autoConnect, onStatusChange, onError]);

  if (!clientRef.current) {
    throw new Error(
      'OrpheusProvider: Must provide either a client instance or config'
    );
  }

  return (
    <OrpheusContext.Provider value={{ client: clientRef.current }}>
      {children}
    </OrpheusContext.Provider>
  );
}

OrpheusProvider.displayName = 'OrpheusProvider';
