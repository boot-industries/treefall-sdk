'use client';

/**
 * useOrpheusEvents Hook
 *
 * Subscribe to Orpheus SDK events in React components
 */

import { useEffect, useState } from 'react';
import type { Event } from '@orpheus/client';
import { useOrpheus } from './useOrpheus.js';

export interface UseOrpheusEventsOptions {
  /** Filter events by type */
  eventTypes?: string[];

  /** Callback when event is received */
  onEvent?: (event: Event) => void;
}

export interface UseOrpheusEventsResult {
  /** Latest event */
  latestEvent: Event | null;

  /** All events received (cleared on unmount) */
  events: Event[];

  /** Clear event history */
  clear: () => void;
}

/**
 * Subscribe to Orpheus SDK events
 *
 * @param options Event subscription options
 *
 * @example
 * ```tsx
 * function SessionMonitor() {
 *   const { latestEvent, events } = useOrpheusEvents({
 *     eventTypes: ['SessionChanged'],
 *     onEvent: (event) => {
 *       console.log('Session event:', event);
 *     },
 *   });
 *
 *   return (
 *     <div>
 *       <h2>Latest Event: {latestEvent?.type}</h2>
 *       <ul>
 *         {events.map((event, i) => (
 *           <li key={i}>{event.type}</li>
 *         ))}
 *       </ul>
 *     </div>
 *   );
 * }
 * ```
 */
export function useOrpheusEvents(
  options: UseOrpheusEventsOptions = {}
): UseOrpheusEventsResult {
  const { client } = useOrpheus();
  const { eventTypes, onEvent } = options;

  const [latestEvent, setLatestEvent] = useState<Event | null>(null);
  const [events, setEvents] = useState<Event[]>([]);

  useEffect(() => {
    const unsubscribe = client.subscribe(
      (event) => {
        setLatestEvent(event);
        setEvents((prev) => [...prev, event]);
        onEvent?.(event);
      },
      { eventTypes }
    );

    return unsubscribe;
  }, [client, eventTypes, onEvent]);

  const clear = () => {
    setLatestEvent(null);
    setEvents([]);
  };

  return {
    latestEvent,
    events,
    clear,
  };
}
