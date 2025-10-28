"use client"

import { useEffect, useRef, useState } from "react"

import { useOrpheusEvents } from "@orpheus/react"

import { Orb, type AgentState } from "@/registry/elevenlabs-ui/ui/orb"

/**
 * OrpheusOrb - Orb visualization integrated with Orpheus transport state
 *
 * Phase 2 Task P2.UI.003
 *
 * Features:
 * - Visualizes transport state (playing/stopped)
 * - Beat-synced animation using TransportTick events
 * - Tempo-responsive motion (faster at higher BPM)
 * - Color changes based on render state
 *
 * Responds to events:
 * - TransportTick: Updates visualization based on position and beat
 * - RenderProgress: Visual feedback during rendering
 * - SessionChanged: Color shift on session load
 */
export function OrpheusOrb({ className }: { className?: string }) {
  const { latestEvent } = useOrpheusEvents()
  const [orbState, setOrbState] = useState<{
    agentState: AgentState
    colors: [string, string]
    manualOutput: number
  }>({
    agentState: null,
    colors: ["#CADCFC", "#A0B9D1"],
    manualOutput: 0.3,
  })

  const lastBeatRef = useRef<number | undefined>(undefined)
  const renderingRef = useRef(false)

  useEffect(() => {
    if (!latestEvent) return

    switch (latestEvent.type) {
      case "TransportTick": {
        // Transport is playing
        const beat = latestEvent.beat
        const tempo = latestEvent.tempo || 120

        // Beat-synced pulse: increase output on downbeat
        if (beat !== undefined && beat !== lastBeatRef.current) {
          lastBeatRef.current = beat
          const isDownbeat = beat % 4 === 0

          // Pulse intensity based on tempo (faster = more intense)
          const tempoFactor = Math.min(tempo / 120, 2.0) // Normalize to 120 BPM
          const pulseIntensity = isDownbeat ? 0.85 : 0.65
          const scaledIntensity = pulseIntensity * tempoFactor

          setOrbState({
            agentState: "talking", // Animated state
            colors: ["#A0D9FF", "#6BB6FF"], // Blue tones for active playback
            manualOutput: scaledIntensity,
          })

          // Decay pulse back to baseline
          setTimeout(() => {
            setOrbState((prev) => ({
              ...prev,
              manualOutput: 0.45,
            }))
          }, (60 / tempo) * 500) // Half a beat duration
        }
        break
      }

      case "RenderProgress": {
        // Rendering in progress
        renderingRef.current = true
        const progress = latestEvent.percentage / 100

        setOrbState({
          agentState: "thinking", // Processing state
          colors: ["#FFD6A0", "#FFAB6B"], // Orange tones for rendering
          manualOutput: 0.5 + progress * 0.3, // Gradually increase
        })
        break
      }

      case "RenderDone": {
        // Render complete
        renderingRef.current = false

        setOrbState({
          agentState: "listening", // Success state
          colors: ["#A0FFC6", "#6BFF9B"], // Green tones for completion
          manualOutput: 0.8,
        })

        // Return to idle after 2 seconds
        setTimeout(() => {
          setOrbState({
            agentState: null,
            colors: ["#CADCFC", "#A0B9D1"],
            manualOutput: 0.3,
          })
        }, 2000)
        break
      }

      case "SessionChanged": {
        // Session loaded
        setOrbState({
          agentState: "listening", // Active state
          colors: ["#D6A0FF", "#AB6BFF"], // Purple tones for session change
          manualOutput: 0.7,
        })

        // Return to idle after 1 second if not rendering
        setTimeout(() => {
          if (!renderingRef.current) {
            setOrbState({
              agentState: null,
              colors: ["#CADCFC", "#A0B9D1"],
              manualOutput: 0.3,
            })
          }
        }, 1000)
        break
      }

      case "Error": {
        // Error occurred
        setOrbState({
          agentState: null,
          colors: ["#FFA0A0", "#FF6B6B"], // Red tones for errors
          manualOutput: 0.5,
        })

        // Return to idle after 3 seconds
        setTimeout(() => {
          setOrbState({
            agentState: null,
            colors: ["#CADCFC", "#A0B9D1"],
            manualOutput: 0.3,
          })
        }, 3000)
        break
      }

      case "Heartbeat": {
        // Periodic heartbeat - subtle pulse if idle
        if (orbState.agentState === null) {
          setOrbState((prev) => ({
            ...prev,
            manualOutput: 0.35,
          }))

          setTimeout(() => {
            setOrbState((prev) => ({
              ...prev,
              manualOutput: 0.3,
            }))
          }, 200)
        }
        break
      }
    }
  }, [latestEvent, orbState.agentState])

  return (
    <Orb
      className={className}
      colors={orbState.colors}
      agentState={orbState.agentState}
      volumeMode="manual"
      manualOutput={orbState.manualOutput}
      manualInput={0}
    />
  )
}
