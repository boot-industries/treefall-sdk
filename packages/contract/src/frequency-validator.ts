/**
 * Event Frequency Validator
 *
 * Validates that events are not emitted more frequently than their defined limits.
 * Used by drivers to ensure compliance with ORP062 frequency constraints.
 */

import { Event, EVENT_FREQUENCY_LIMITS } from './v1.0.0-beta.js';

export interface FrequencyViolation {
  eventType: string;
  limit: number;
  actual: number;
  timestamp: number;
}

export class EventFrequencyValidator {
  private lastEmitTimes: Map<string, number> = new Map();
  private violations: FrequencyViolation[] = [];
  private enabled: boolean = true;

  constructor(options?: { enabled?: boolean }) {
    if (options?.enabled !== undefined) {
      this.enabled = options.enabled;
    }
  }

  /**
   * Check if an event can be emitted according to frequency constraints.
   * Returns true if the event is allowed, false if it violates the limit.
   */
  canEmit(event: Event): boolean {
    if (!this.enabled) {
      return true;
    }

    // Only validate events with defined frequency limits
    const eventType = event.type;
    const limit = EVENT_FREQUENCY_LIMITS[eventType as keyof typeof EVENT_FREQUENCY_LIMITS];

    if (limit === undefined) {
      // No frequency limit defined for this event type
      return true;
    }

    const now = event.timestamp || Date.now();
    const lastEmitTime = this.lastEmitTimes.get(eventType);

    if (lastEmitTime === undefined) {
      // First emission of this event type
      this.lastEmitTimes.set(eventType, now);
      return true;
    }

    // Calculate minimum interval in milliseconds
    const minInterval = 1000 / limit;
    const actualInterval = now - lastEmitTime;

    // Use a small epsilon for floating point comparison
    const epsilon = 0.001; // 1 microsecond tolerance

    if (actualInterval < minInterval - epsilon) {
      // Frequency limit violated
      this.violations.push({
        eventType,
        limit,
        actual: 1000 / actualInterval,
        timestamp: now,
      });
      return false;
    }

    // Update last emit time
    this.lastEmitTimes.set(eventType, now);
    return true;
  }

  /**
   * Record that an event was emitted (for tracking only, does not validate).
   * Use this after successfully emitting an event that passed canEmit().
   */
  recordEmit(event: Event): void {
    if (!this.enabled) {
      return;
    }

    const eventType = event.type;
    const now = event.timestamp || Date.now();
    this.lastEmitTimes.set(eventType, now);
  }

  /**
   * Get all frequency violations that have occurred.
   */
  getViolations(): FrequencyViolation[] {
    return [...this.violations];
  }

  /**
   * Clear all violation records and reset state.
   */
  reset(): void {
    this.lastEmitTimes.clear();
    this.violations = [];
  }

  /**
   * Enable or disable validation.
   */
  setEnabled(enabled: boolean): void {
    this.enabled = enabled;
  }

  /**
   * Get the maximum allowed frequency for an event type (in Hz).
   * Returns undefined if no limit is defined.
   */
  static getLimit(eventType: string): number | undefined {
    return EVENT_FREQUENCY_LIMITS[eventType as keyof typeof EVENT_FREQUENCY_LIMITS];
  }

  /**
   * Get the minimum interval between emissions for an event type (in milliseconds).
   * Returns 0 if no limit is defined.
   */
  static getMinInterval(eventType: string): number {
    const limit = EventFrequencyValidator.getLimit(eventType);
    return limit ? 1000 / limit : 0;
  }
}

/**
 * Create a throttled event emitter that respects frequency limits.
 *
 * Example usage:
 * ```typescript
 * const emitter = createThrottledEmitter((event) => {
 *   // Send event to client
 *   sendEvent(event);
 * });
 *
 * // These calls will be automatically throttled
 * emitter({ type: 'TransportTick', timestamp: Date.now(), position: 100 });
 * emitter({ type: 'TransportTick', timestamp: Date.now(), position: 200 });
 * ```
 */
export function createThrottledEmitter(
  emit: (event: Event) => void,
  options?: { enabled?: boolean }
): (event: Event) => boolean {
  const validator = new EventFrequencyValidator(options);

  return (event: Event): boolean => {
    if (validator.canEmit(event)) {
      emit(event);
      validator.recordEmit(event);
      return true;
    }
    return false;
  };
}
