/**
 * Tests for EventFrequencyValidator
 */

import { describe, it, expect, beforeEach, vi } from 'vitest';
import {
  EventFrequencyValidator,
  createThrottledEmitter,
  type FrequencyViolation,
} from './frequency-validator.js';
import { Event, EVENT_FREQUENCY_LIMITS } from './v1.0.0-beta.js';

describe('EventFrequencyValidator', () => {
  let validator: EventFrequencyValidator;

  beforeEach(() => {
    validator = new EventFrequencyValidator();
  });

  describe('canEmit', () => {
    it('should allow first emission of any event', () => {
      const event: Event = {
        type: 'TransportTick',
        timestamp: 1000,
        position: 0,
      };

      expect(validator.canEmit(event)).toBe(true);
    });

    it('should allow events with no frequency limit', () => {
      const event1: Event = {
        type: 'SessionChanged',
        timestamp: 1000,
      };

      const event2: Event = {
        type: 'SessionChanged',
        timestamp: 1001, // 1ms later
      };

      expect(validator.canEmit(event1)).toBe(true);
      expect(validator.canEmit(event2)).toBe(true);
    });

    it('should enforce TransportTick frequency limit (30 Hz)', () => {
      const limit = EVENT_FREQUENCY_LIMITS.TransportTick; // 30 Hz
      const minInterval = 1000 / limit; // ~33.33ms

      const event1: Event = {
        type: 'TransportTick',
        timestamp: 1000,
        position: 0,
      };

      const event2: Event = {
        type: 'TransportTick',
        timestamp: 1000 + minInterval - 1, // Too soon
        position: 100,
      };

      const event3: Event = {
        type: 'TransportTick',
        timestamp: 1000 + minInterval, // Exactly at limit
        position: 200,
      };

      expect(validator.canEmit(event1)).toBe(true);
      expect(validator.canEmit(event2)).toBe(false); // Violates limit
      expect(validator.canEmit(event3)).toBe(true);
    });

    it('should enforce RenderProgress frequency limit (10 Hz)', () => {
      const limit = EVENT_FREQUENCY_LIMITS.RenderProgress; // 10 Hz
      const minInterval = 1000 / limit; // 100ms

      const event1: Event = {
        type: 'RenderProgress',
        timestamp: 1000,
        percentage: 0,
      };

      const event2: Event = {
        type: 'RenderProgress',
        timestamp: 1050, // 50ms later - too soon
        percentage: 50,
      };

      const event3: Event = {
        type: 'RenderProgress',
        timestamp: 1100, // 100ms later - OK
        percentage: 100,
      };

      expect(validator.canEmit(event1)).toBe(true);
      expect(validator.canEmit(event2)).toBe(false); // Violates limit
      expect(validator.canEmit(event3)).toBe(true);
    });

    it('should enforce Heartbeat frequency limit (0.1 Hz)', () => {
      const limit = EVENT_FREQUENCY_LIMITS.Heartbeat; // 0.1 Hz
      const minInterval = 1000 / limit; // 10000ms (10 seconds)

      const event1: Event = {
        type: 'Heartbeat',
        timestamp: 1000,
      };

      const event2: Event = {
        type: 'Heartbeat',
        timestamp: 5000, // 4s later - too soon
      };

      const event3: Event = {
        type: 'Heartbeat',
        timestamp: 11000, // 10s later - OK
      };

      expect(validator.canEmit(event1)).toBe(true);
      expect(validator.canEmit(event2)).toBe(false); // Violates limit
      expect(validator.canEmit(event3)).toBe(true);
    });

    it('should track violations', () => {
      const event1: Event = {
        type: 'TransportTick',
        timestamp: 1000,
        position: 0,
      };

      const event2: Event = {
        type: 'TransportTick',
        timestamp: 1010, // Too soon
        position: 100,
      };

      validator.canEmit(event1);
      validator.canEmit(event2);

      const violations = validator.getViolations();
      expect(violations.length).toBe(1);
      expect(violations[0].eventType).toBe('TransportTick');
      expect(violations[0].limit).toBe(EVENT_FREQUENCY_LIMITS.TransportTick);
    });

    it('should handle disabled validation', () => {
      const validator = new EventFrequencyValidator({ enabled: false });

      const event1: Event = {
        type: 'TransportTick',
        timestamp: 1000,
        position: 0,
      };

      const event2: Event = {
        type: 'TransportTick',
        timestamp: 1001, // Immediate - would violate if enabled
        position: 100,
      };

      expect(validator.canEmit(event1)).toBe(true);
      expect(validator.canEmit(event2)).toBe(true); // Still allowed when disabled
    });
  });

  describe('recordEmit', () => {
    it('should update last emit time', () => {
      const event1: Event = {
        type: 'TransportTick',
        timestamp: 1000,
        position: 0,
      };

      validator.recordEmit(event1);

      const event2: Event = {
        type: 'TransportTick',
        timestamp: 1010, // Too soon
        position: 100,
      };

      expect(validator.canEmit(event2)).toBe(false);
    });
  });

  describe('reset', () => {
    it('should clear all state', () => {
      const event1: Event = {
        type: 'TransportTick',
        timestamp: 1000,
        position: 0,
      };

      const event2: Event = {
        type: 'TransportTick',
        timestamp: 1010, // Too soon
        position: 100,
      };

      validator.canEmit(event1);
      validator.canEmit(event2); // Creates violation

      expect(validator.getViolations().length).toBe(1);

      validator.reset();

      expect(validator.getViolations().length).toBe(0);
      expect(validator.canEmit(event1)).toBe(true); // Can emit again after reset
    });
  });

  describe('static methods', () => {
    it('should get limit for event type', () => {
      expect(EventFrequencyValidator.getLimit('TransportTick')).toBe(30);
      expect(EventFrequencyValidator.getLimit('RenderProgress')).toBe(10);
      expect(EventFrequencyValidator.getLimit('Heartbeat')).toBe(0.1);
      expect(EventFrequencyValidator.getLimit('SessionChanged')).toBeUndefined();
    });

    it('should get min interval for event type', () => {
      expect(EventFrequencyValidator.getMinInterval('TransportTick')).toBeCloseTo(33.33, 2);
      expect(EventFrequencyValidator.getMinInterval('RenderProgress')).toBe(100);
      expect(EventFrequencyValidator.getMinInterval('Heartbeat')).toBe(10000);
      expect(EventFrequencyValidator.getMinInterval('SessionChanged')).toBe(0);
    });
  });
});

describe('createThrottledEmitter', () => {
  it('should throttle event emissions', () => {
    const emittedEvents: Event[] = [];
    const emit = vi.fn((event: Event) => {
      emittedEvents.push(event);
    });

    const throttledEmit = createThrottledEmitter(emit);

    const event1: Event = {
      type: 'TransportTick',
      timestamp: 1000,
      position: 0,
    };

    const event2: Event = {
      type: 'TransportTick',
      timestamp: 1010, // Too soon
      position: 100,
    };

    const event3: Event = {
      type: 'TransportTick',
      timestamp: 1034, // OK (>33.33ms later)
      position: 200,
    };

    expect(throttledEmit(event1)).toBe(true);
    expect(throttledEmit(event2)).toBe(false); // Throttled
    expect(throttledEmit(event3)).toBe(true);

    expect(emit).toHaveBeenCalledTimes(2);
    expect(emittedEvents).toEqual([event1, event3]);
  });

  it('should respect disabled flag', () => {
    const emit = vi.fn();
    const throttledEmit = createThrottledEmitter(emit, { enabled: false });

    const event1: Event = {
      type: 'TransportTick',
      timestamp: 1000,
      position: 0,
    };

    const event2: Event = {
      type: 'TransportTick',
      timestamp: 1001, // Immediate - would violate if enabled
      position: 100,
    };

    expect(throttledEmit(event1)).toBe(true);
    expect(throttledEmit(event2)).toBe(true); // Not throttled

    expect(emit).toHaveBeenCalledTimes(2);
  });
});
