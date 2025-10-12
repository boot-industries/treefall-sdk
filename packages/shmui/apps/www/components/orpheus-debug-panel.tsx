"use client"

import { useState } from "react"

import { useOrpheusCommand, useOrpheusEvents } from "@orpheus/react"

import { Button } from "@/registry/elevenlabs-ui/ui/button"
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/registry/elevenlabs-ui/ui/card"

/**
 * OrpheusDebugPanel - Development-only debug panel for testing Orpheus SDK integration
 *
 * Features:
 * - Get Core Version: Calls GetVersion command (placeholder for now)
 * - Load Test Session: Loads a test session from fixtures
 * - Real-time event display
 * - Error handling with detailed messages
 *
 * Only visible when NODE_ENV === 'development'
 */
export function OrpheusDebugPanel() {
  // Only render in development mode
  if (process.env.NODE_ENV !== "development") {
    return null
  }

  const { execute, loading, error } = useOrpheusCommand()
  const { latestEvent, events } = useOrpheusEvents()
  const [lastResult, setLastResult] = useState<{
    command: string
    result: unknown
    timestamp: string
  } | null>(null)

  const handleGetVersion = async () => {
    try {
      // Note: GetVersion command placeholder - will be implemented in Phase 2
      const result = await execute({
        type: "GetVersion" as "LoadSession", // Type cast for now until GetVersion is added
      })
      setLastResult({
        command: "GetVersion",
        result,
        timestamp: new Date().toLocaleTimeString(),
      })
    } catch (err) {
      console.error("GetVersion failed:", err)
    }
  }

  const handleLoadTestSession = async () => {
    try {
      // Load the solo_click.json fixture
      const result = await execute({
        type: "LoadSession",
        sessionPath: "../../../tools/fixtures/solo_click.json",
      })
      setLastResult({
        command: "LoadSession",
        result,
        timestamp: new Date().toLocaleTimeString(),
      })
    } catch (err) {
      console.error("LoadSession failed:", err)
    }
  }

  return (
    <Card className="fixed bottom-4 right-4 z-50 w-96 border-2 border-yellow-500 bg-background/95 shadow-xl backdrop-blur supports-[backdrop-filter]:bg-background/80">
      <CardHeader className="pb-3">
        <CardTitle className="text-sm font-semibold text-yellow-600 dark:text-yellow-400">
          Orpheus Debug Panel
        </CardTitle>
        <CardDescription className="text-xs">
          Development mode only • P1.UI.001
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-3">
        {/* Action Buttons */}
        <div className="flex gap-2">
          <Button
            size="sm"
            variant="outline"
            onClick={handleGetVersion}
            disabled={loading}
            className="flex-1"
          >
            {loading ? "Loading..." : "Get Version"}
          </Button>
          <Button
            size="sm"
            variant="outline"
            onClick={handleLoadTestSession}
            disabled={loading}
            className="flex-1"
          >
            {loading ? "Loading..." : "Load Test Session"}
          </Button>
        </div>

        {/* Error Display */}
        {error && (
          <div className="rounded-md border border-red-500 bg-red-50 p-2 text-xs text-red-700 dark:bg-red-950/30 dark:text-red-400">
            <div className="font-semibold">Error:</div>
            <div className="mt-1 font-mono">{error.message}</div>
          </div>
        )}

        {/* Last Result Display */}
        {lastResult && (
          <div className="rounded-md border bg-muted p-2 text-xs">
            <div className="mb-1 flex items-center justify-between">
              <span className="font-semibold">{lastResult.command}</span>
              <span className="text-muted-foreground">
                {lastResult.timestamp}
              </span>
            </div>
            <pre className="overflow-auto whitespace-pre-wrap font-mono text-[10px]">
              {JSON.stringify(lastResult.result, null, 2)}
            </pre>
          </div>
        )}

        {/* Latest Event Display */}
        {latestEvent && (
          <div className="rounded-md border border-blue-500 bg-blue-50 p-2 text-xs text-blue-700 dark:bg-blue-950/30 dark:text-blue-400">
            <div className="mb-1 flex items-center justify-between">
              <span className="font-semibold">Latest Event:</span>
              <span className="text-xs opacity-75">{latestEvent.type}</span>
            </div>
            <pre className="overflow-auto whitespace-pre-wrap font-mono text-[10px]">
              {JSON.stringify(latestEvent, null, 2)}
            </pre>
          </div>
        )}

        {/* Event Counter */}
        {events.length > 0 && (
          <div className="text-center text-xs text-muted-foreground">
            {events.length} event{events.length !== 1 ? "s" : ""} received
          </div>
        )}

        {/* Loading Indicator */}
        {loading && (
          <div className="flex items-center justify-center gap-2 text-xs text-muted-foreground">
            <div className="h-3 w-3 animate-spin rounded-full border-2 border-current border-t-transparent" />
            <span>Executing command...</span>
          </div>
        )}

        {/* Initial State */}
        {!lastResult && !error && !loading && !latestEvent && (
          <div className="text-center text-xs text-muted-foreground">
            Click a button to test Orpheus SDK integration
          </div>
        )}
      </CardContent>
    </Card>
  )
}
