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
import { Input } from "@/registry/elevenlabs-ui/ui/input"
import { Label } from "@/registry/elevenlabs-ui/ui/label"

/**
 * OrpheusSessionManager - UI panel for session management
 *
 * Phase 2 Task P2.UI.001
 *
 * Features:
 * - Load session from file path
 * - Save session to file path
 * - Display session metadata (track count, tempo, etc.)
 * - Real-time session state updates
 *
 * Uses contract v1.0.0-beta commands:
 * - LoadSession
 * - SaveSession
 *
 * Responds to events:
 * - SessionChanged
 */
export function OrpheusSessionManager() {
  const { execute, loading, error } = useOrpheusCommand()
  const { latestEvent } = useOrpheusEvents()

  const [sessionPath, setSessionPath] = useState<string>(
    "../../../tools/fixtures/solo_click.json"
  )
  const [saveSessionPath, setSaveSessionPath] = useState<string>("")
  const [sessionState, setSessionState] = useState<{
    loaded: boolean
    path?: string
    trackCount?: number
    tempo?: number
    lastUpdated?: string
  }>({
    loaded: false,
  })

  // Update session state when SessionChanged event received
  if (
    latestEvent &&
    latestEvent.type === "SessionChanged" &&
    sessionState.lastUpdated !==
      new Date(latestEvent.timestamp).toISOString()
  ) {
    setSessionState({
      loaded: true,
      path: latestEvent.sessionPath,
      trackCount: latestEvent.trackCount,
      lastUpdated: new Date(latestEvent.timestamp).toISOString(),
    })
  }

  const handleLoadSession = async () => {
    if (!sessionPath.trim()) {
      return
    }

    try {
      await execute({
        type: "LoadSession",
        path: sessionPath,
      })
    } catch (err) {
      console.error("LoadSession failed:", err)
    }
  }

  const handleSaveSession = async () => {
    try {
      await execute({
        type: "SaveSession",
        path: saveSessionPath.trim() || undefined,
      })

      // Update UI to show save success
      if (saveSessionPath.trim()) {
        setSessionState((prev) => ({
          ...prev,
          path: saveSessionPath,
          lastUpdated: new Date().toISOString(),
        }))
      }
    } catch (err) {
      console.error("SaveSession failed:", err)
    }
  }

  const handleNewSession = async () => {
    // Reset UI state
    setSessionState({
      loaded: false,
    })
    setSessionPath("")
    setSaveSessionPath("")

    // TODO: Once SDK supports it, call NewSession command
    // For now, just reset UI state
  }

  return (
    <Card className="w-full max-w-2xl">
      <CardHeader>
        <CardTitle>Session Manager</CardTitle>
        <CardDescription>
          Load, save, and manage Orpheus sessions (P2.UI.001)
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-6">
        {/* Session Status */}
        {sessionState.loaded && (
          <div className="rounded-md border bg-muted p-4">
            <h3 className="mb-2 text-sm font-semibold">Current Session</h3>
            <div className="space-y-1 text-sm">
              {sessionState.path && (
                <div className="flex items-center justify-between">
                  <span className="text-muted-foreground">Path:</span>
                  <span className="truncate font-mono text-xs">
                    {sessionState.path}
                  </span>
                </div>
              )}
              {sessionState.trackCount !== undefined && (
                <div className="flex items-center justify-between">
                  <span className="text-muted-foreground">Tracks:</span>
                  <span>{sessionState.trackCount}</span>
                </div>
              )}
              {sessionState.tempo && (
                <div className="flex items-center justify-between">
                  <span className="text-muted-foreground">Tempo:</span>
                  <span>{sessionState.tempo} BPM</span>
                </div>
              )}
              {sessionState.lastUpdated && (
                <div className="flex items-center justify-between">
                  <span className="text-muted-foreground">Last Updated:</span>
                  <span className="text-xs">
                    {new Date(sessionState.lastUpdated).toLocaleTimeString()}
                  </span>
                </div>
              )}
            </div>
          </div>
        )}

        {/* Load Session Section */}
        <div className="space-y-2">
          <Label htmlFor="session-path">Load Session</Label>
          <div className="flex gap-2">
            <Input
              id="session-path"
              type="text"
              placeholder="Path to session file..."
              value={sessionPath}
              onChange={(e) => setSessionPath(e.target.value)}
              disabled={loading}
              className="flex-1"
            />
            <Button
              onClick={handleLoadSession}
              disabled={loading || !sessionPath.trim()}
            >
              {loading ? "Loading..." : "Load"}
            </Button>
          </div>
          <p className="text-xs text-muted-foreground">
            Load an existing Orpheus session file (.json)
          </p>
        </div>

        {/* Save Session Section */}
        <div className="space-y-2">
          <Label htmlFor="save-session-path">Save Session</Label>
          <div className="flex gap-2">
            <Input
              id="save-session-path"
              type="text"
              placeholder="Path to save session (optional)..."
              value={saveSessionPath}
              onChange={(e) => setSaveSessionPath(e.target.value)}
              disabled={loading}
              className="flex-1"
            />
            <Button
              onClick={handleSaveSession}
              disabled={loading}
              variant="outline"
            >
              {loading ? "Saving..." : "Save"}
            </Button>
          </div>
          <p className="text-xs text-muted-foreground">
            Save current session. Leave empty to overwrite existing file.
          </p>
        </div>

        {/* Actions */}
        <div className="flex gap-2">
          <Button onClick={handleNewSession} variant="outline" disabled={loading}>
            New Session
          </Button>
        </div>

        {/* Error Display */}
        {error && (
          <div className="rounded-md border border-red-500 bg-red-50 p-3 text-sm text-red-700 dark:bg-red-950/30 dark:text-red-400">
            <div className="font-semibold">Error</div>
            <div className="mt-1 font-mono text-xs">{error.message}</div>
            {error.details && (
              <pre className="mt-2 overflow-auto whitespace-pre-wrap text-[10px]">
                {JSON.stringify(error.details, null, 2)}
              </pre>
            )}
          </div>
        )}

        {/* Loading Indicator */}
        {loading && (
          <div className="flex items-center justify-center gap-2 text-sm text-muted-foreground">
            <div className="h-4 w-4 animate-spin rounded-full border-2 border-current border-t-transparent" />
            <span>Processing...</span>
          </div>
        )}

        {/* Initial State */}
        {!sessionState.loaded && !error && !loading && (
          <div className="rounded-md border border-dashed p-8 text-center">
            <p className="text-sm text-muted-foreground">
              No session loaded. Load an existing session or create a new one.
            </p>
          </div>
        )}
      </CardContent>
    </Card>
  )
}
