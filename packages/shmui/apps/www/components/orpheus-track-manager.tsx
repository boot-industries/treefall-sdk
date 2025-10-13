"use client"

import { useEffect, useState } from "react"

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
import { ScrollArea } from "@/registry/elevenlabs-ui/ui/scroll-area"

/**
 * OrpheusTrackManager - UI panel for track management
 *
 * Phase 2 Task P2.UI.004
 *
 * Features:
 * - Display list of tracks in current session
 * - Add new audio/MIDI tracks
 * - Remove existing tracks
 * - Track metadata display (name, type, channel count)
 *
 * NOTE: This component is ready for future SDK commands:
 * - AddTrack (not yet implemented in contract v1.0.0-beta)
 * - RemoveTrack (not yet implemented in contract v1.0.0-beta)
 * - GetSessionInfo (not yet implemented in contract v1.0.0-beta)
 *
 * Currently uses SessionChanged event for track count updates.
 * Full track list functionality will be enabled when SDK commands are available.
 *
 * Responds to events:
 * - SessionChanged: Updates track count
 */
export function OrpheusTrackManager() {
  const { loading, error } = useOrpheusCommand()
  const { latestEvent } = useOrpheusEvents()

  const [trackCount, setTrackCount] = useState<number>(0)
  const [newTrackName, setNewTrackName] = useState<string>("")
  const [newTrackType, setNewTrackType] = useState<"audio" | "midi">("audio")
  const [tracks, setTracks] = useState<
    Array<{
      id: string
      name: string
      type: "audio" | "midi"
      channels?: number
    }>
  >([])

  // Update track count when SessionChanged event received
  useEffect(() => {
    if (latestEvent && latestEvent.type === "SessionChanged") {
      if (latestEvent.trackCount !== undefined) {
        setTrackCount(latestEvent.trackCount)

        // TODO: Once SDK supports GetSessionInfo, fetch full track list
        // For now, generate placeholder track data
        if (latestEvent.trackCount > 0) {
          const placeholderTracks = Array.from(
            { length: latestEvent.trackCount },
            (_, i) => ({
              id: `track-${i + 1}`,
              name: `Track ${i + 1}`,
              type: i % 2 === 0 ? ("audio" as const) : ("midi" as const),
              channels: i % 2 === 0 ? 2 : undefined,
            })
          )
          setTracks(placeholderTracks)
        } else {
          setTracks([])
        }
      }
    }
  }, [latestEvent])

  const handleAddTrack = async () => {
    if (!newTrackName.trim()) {
      return
    }

    try {
      // TODO: Implement AddTrack command when available in SDK
      // await execute({
      //   type: 'AddTrack',
      //   name: newTrackName,
      //   trackType: newTrackType,
      //   channels: newTrackType === 'audio' ? 2 : undefined,
      // });

      // For now, simulate track addition in UI only
      const newTrack = {
        id: `track-${Date.now()}`,
        name: newTrackName,
        type: newTrackType,
        channels: newTrackType === "audio" ? 2 : undefined,
      }

      setTracks((prev) => [...prev, newTrack])
      setTrackCount((prev) => prev + 1)
      setNewTrackName("")

      console.log(
        "[OrpheusTrackManager] AddTrack command not yet implemented in SDK. UI-only update."
      )
    } catch (err) {
      console.error("AddTrack failed:", err)
    }
  }

  const handleRemoveTrack = async (trackId: string) => {
    try {
      // TODO: Implement RemoveTrack command when available in SDK
      // await execute({
      //   type: 'RemoveTrack',
      //   trackId: trackId,
      // });

      // For now, simulate track removal in UI only
      setTracks((prev) => prev.filter((t) => t.id !== trackId))
      setTrackCount((prev) => Math.max(0, prev - 1))

      console.log(
        "[OrpheusTrackManager] RemoveTrack command not yet implemented in SDK. UI-only update."
      )
    } catch (err) {
      console.error("RemoveTrack failed:", err)
    }
  }

  return (
    <Card className="w-full max-w-2xl">
      <CardHeader>
        <CardTitle>Track Manager</CardTitle>
        <CardDescription>
          Add and remove tracks in the current session (P2.UI.004)
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-6">
        {/* Track Count Summary */}
        <div className="rounded-md border bg-muted p-4">
          <div className="flex items-center justify-between">
            <span className="text-sm font-semibold">Total Tracks:</span>
            <span className="text-2xl font-bold">{trackCount}</span>
          </div>
          {trackCount === 0 && (
            <p className="mt-2 text-xs text-muted-foreground">
              No tracks in session. Add a track to get started.
            </p>
          )}
        </div>

        {/* Add Track Section */}
        <div className="space-y-3">
          <Label htmlFor="new-track-name">Add New Track</Label>
          <div className="flex gap-2">
            <Input
              id="new-track-name"
              type="text"
              placeholder="Track name..."
              value={newTrackName}
              onChange={(e) => setNewTrackName(e.target.value)}
              disabled={loading}
              className="flex-1"
            />
            <select
              value={newTrackType}
              onChange={(e) =>
                setNewTrackType(e.target.value as "audio" | "midi")
              }
              disabled={loading}
              className="rounded-md border bg-background px-3 py-2 text-sm"
            >
              <option value="audio">Audio</option>
              <option value="midi">MIDI</option>
            </select>
            <Button
              onClick={handleAddTrack}
              disabled={loading || !newTrackName.trim()}
            >
              Add
            </Button>
          </div>
          <p className="text-xs text-muted-foreground">
            Note: AddTrack command will be implemented in a future SDK release.
            Currently UI-only.
          </p>
        </div>

        {/* Track List */}
        {tracks.length > 0 && (
          <div className="space-y-2">
            <Label>Current Tracks</Label>
            <ScrollArea className="h-64 rounded-md border">
              <div className="space-y-2 p-4">
                {tracks.map((track) => (
                  <div
                    key={track.id}
                    className="flex items-center justify-between rounded-md border bg-card p-3"
                  >
                    <div className="flex-1">
                      <div className="flex items-center gap-2">
                        <span className="font-semibold">{track.name}</span>
                        <span
                          className={`rounded-full px-2 py-0.5 text-xs ${
                            track.type === "audio"
                              ? "bg-blue-100 text-blue-700 dark:bg-blue-950 dark:text-blue-400"
                              : "bg-purple-100 text-purple-700 dark:bg-purple-950 dark:text-purple-400"
                          }`}
                        >
                          {track.type}
                        </span>
                      </div>
                      {track.channels && (
                        <p className="mt-1 text-xs text-muted-foreground">
                          {track.channels} channel{track.channels !== 1 ? "s" : ""}
                        </p>
                      )}
                    </div>
                    <Button
                      size="sm"
                      variant="ghost"
                      onClick={() => handleRemoveTrack(track.id)}
                      disabled={loading}
                      className="text-red-600 hover:bg-red-50 hover:text-red-700 dark:text-red-400 dark:hover:bg-red-950"
                    >
                      Remove
                    </Button>
                  </div>
                ))}
              </div>
            </ScrollArea>
          </div>
        )}

        {/* Error Display */}
        {error && (
          <div className="rounded-md border border-red-500 bg-red-50 p-3 text-sm text-red-700 dark:bg-red-950/30 dark:text-red-400">
            <div className="font-semibold">Error</div>
            <div className="mt-1 font-mono text-xs">{error.message}</div>
          </div>
        )}

        {/* SDK Implementation Note */}
        <div className="rounded-md border border-yellow-500 bg-yellow-50 p-3 text-xs text-yellow-700 dark:bg-yellow-950/30 dark:text-yellow-400">
          <div className="font-semibold">Development Note</div>
          <div className="mt-1">
            This component is ready for future SDK commands (AddTrack,
            RemoveTrack, GetSessionInfo). Currently operates in UI-only mode
            for demonstration purposes. Full functionality will be enabled when
            these commands are added to the contract and implemented in the SDK.
          </div>
        </div>

        {/* Loading Indicator */}
        {loading && (
          <div className="flex items-center justify-center gap-2 text-sm text-muted-foreground">
            <div className="h-4 w-4 animate-spin rounded-full border-2 border-current border-t-transparent" />
            <span>Processing...</span>
          </div>
        )}
      </CardContent>
    </Card>
  )
}
