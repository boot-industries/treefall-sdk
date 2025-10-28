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
import { Slider } from "@/registry/elevenlabs-ui/ui/slider"
import { Progress } from "@/registry/elevenlabs-ui/ui/progress"

/**
 * OrpheusClickGenerator - UI panel for click track generation
 *
 * Phase 2 Task P2.UI.002
 *
 * Features:
 * - Configure BPM (40-240)
 * - Configure bar count (1-16)
 * - Set output file path
 * - Render click track
 * - Show render progress (RenderProgress events)
 * - Display completion status (RenderDone event)
 *
 * Uses contract v1.0.0-beta commands:
 * - RenderClick
 *
 * Responds to events:
 * - RenderProgress (progress percentage updates)
 * - RenderDone (render completion notification)
 * - Error (render errors)
 */
export function OrpheusClickGenerator() {
  const { execute, loading, error } = useOrpheusCommand()
  const { latestEvent } = useOrpheusEvents()

  const [outputPath, setOutputPath] = useState<string>(
    "/tmp/click_track.wav"
  )
  const [bpm, setBpm] = useState<number>(120)
  const [bars, setBars] = useState<number>(4)
  const [renderState, setRenderState] = useState<{
    rendering: boolean
    progress: number
    renderId?: string
    outputPath?: string
    duration?: number
    sampleRate?: number
    channels?: number
  }>({
    rendering: false,
    progress: 0,
  })

  // Update render state when RenderProgress event received
  if (
    latestEvent &&
    latestEvent.type === "RenderProgress" &&
    renderState.rendering
  ) {
    const progress = latestEvent.percentage
    if (progress !== renderState.progress) {
      setRenderState((prev) => ({
        ...prev,
        progress,
        renderId: latestEvent.renderId,
      }))
    }
  }

  // Update render state when RenderDone event received
  if (latestEvent && latestEvent.type === "RenderDone") {
    setRenderState({
      rendering: false,
      progress: 100,
      renderId: latestEvent.renderId,
      outputPath: latestEvent.outputPath,
      duration: latestEvent.duration,
      sampleRate: latestEvent.sampleRate,
      channels: latestEvent.channels,
    })
  }

  const handleRenderClick = async () => {
    if (!outputPath.trim()) {
      return
    }

    // Reset render state
    setRenderState({
      rendering: true,
      progress: 0,
    })

    try {
      await execute({
        type: "RenderClick",
        outputPath: outputPath,
        bars: bars,
        bpm: bpm,
      })
    } catch (err) {
      console.error("RenderClick failed:", err)
      setRenderState({
        rendering: false,
        progress: 0,
      })
    }
  }

  const formatDuration = (seconds: number | undefined): string => {
    if (!seconds) return "-"
    const mins = Math.floor(seconds / 60)
    const secs = (seconds % 60).toFixed(1)
    return mins > 0 ? `${mins}:${secs.padStart(4, "0")}` : `${secs}s`
  }

  return (
    <Card className="w-full max-w-2xl">
      <CardHeader>
        <CardTitle>Click Track Generator</CardTitle>
        <CardDescription>
          Generate metronome click tracks for recording (P2.UI.002)
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-6">
        {/* Render Status */}
        {renderState.rendering && (
          <div className="rounded-md border bg-blue-50 p-4 dark:bg-blue-950/30">
            <div className="mb-2 flex items-center justify-between">
              <h3 className="text-sm font-semibold text-blue-700 dark:text-blue-400">
                Rendering...
              </h3>
              <span className="text-sm font-mono text-blue-600 dark:text-blue-500">
                {renderState.progress.toFixed(0)}%
              </span>
            </div>
            <Progress value={renderState.progress} className="h-2" />
          </div>
        )}

        {/* Render Complete Status */}
        {!renderState.rendering && renderState.progress === 100 && (
          <div className="rounded-md border border-green-500 bg-green-50 p-4 dark:bg-green-950/30">
            <h3 className="mb-2 text-sm font-semibold text-green-700 dark:text-green-400">
              Render Complete
            </h3>
            <div className="space-y-1 text-xs text-green-600 dark:text-green-500">
              {renderState.outputPath && (
                <div className="flex items-center justify-between">
                  <span>Output:</span>
                  <span className="truncate font-mono">
                    {renderState.outputPath}
                  </span>
                </div>
              )}
              {renderState.duration !== undefined && (
                <div className="flex items-center justify-between">
                  <span>Duration:</span>
                  <span>{formatDuration(renderState.duration)}</span>
                </div>
              )}
              {renderState.sampleRate && (
                <div className="flex items-center justify-between">
                  <span>Sample Rate:</span>
                  <span>{renderState.sampleRate} Hz</span>
                </div>
              )}
              {renderState.channels && (
                <div className="flex items-center justify-between">
                  <span>Channels:</span>
                  <span>{renderState.channels}</span>
                </div>
              )}
            </div>
          </div>
        )}

        {/* Output Path */}
        <div className="space-y-2">
          <Label htmlFor="output-path">Output File Path</Label>
          <Input
            id="output-path"
            type="text"
            placeholder="/path/to/output.wav"
            value={outputPath}
            onChange={(e) => setOutputPath(e.target.value)}
            disabled={loading || renderState.rendering}
          />
          <p className="text-xs text-muted-foreground">
            Destination path for the generated click track (.wav)
          </p>
        </div>

        {/* BPM Slider */}
        <div className="space-y-2">
          <div className="flex items-center justify-between">
            <Label htmlFor="bpm-slider">Tempo (BPM)</Label>
            <span className="text-sm font-mono font-semibold">{bpm}</span>
          </div>
          <Slider
            id="bpm-slider"
            min={40}
            max={240}
            step={1}
            value={[bpm]}
            onValueChange={(values) => setBpm(values[0])}
            disabled={loading || renderState.rendering}
            className="w-full"
          />
          <div className="flex justify-between text-xs text-muted-foreground">
            <span>40 BPM</span>
            <span>240 BPM</span>
          </div>
        </div>

        {/* Bar Count Slider */}
        <div className="space-y-2">
          <div className="flex items-center justify-between">
            <Label htmlFor="bars-slider">Bar Count</Label>
            <span className="text-sm font-mono font-semibold">
              {bars} {bars === 1 ? "bar" : "bars"}
            </span>
          </div>
          <Slider
            id="bars-slider"
            min={1}
            max={16}
            step={1}
            value={[bars]}
            onValueChange={(values) => setBars(values[0])}
            disabled={loading || renderState.rendering}
            className="w-full"
          />
          <div className="flex justify-between text-xs text-muted-foreground">
            <span>1 bar</span>
            <span>16 bars</span>
          </div>
        </div>

        {/* Calculated Duration */}
        <div className="rounded-md border border-dashed p-3 text-sm">
          <div className="flex items-center justify-between">
            <span className="text-muted-foreground">Estimated Duration:</span>
            <span className="font-mono font-semibold">
              {formatDuration((bars * 4 * 60) / bpm)}
            </span>
          </div>
          <p className="mt-1 text-xs text-muted-foreground">
            {bars} bars × 4 beats × {(60 / bpm).toFixed(2)}s per beat
          </p>
        </div>

        {/* Actions */}
        <div className="flex gap-2">
          <Button
            onClick={handleRenderClick}
            disabled={
              loading ||
              renderState.rendering ||
              !outputPath.trim()
            }
            className="flex-1"
          >
            {renderState.rendering
              ? "Rendering..."
              : loading
                ? "Processing..."
                : "Render Click Track"}
          </Button>
          {renderState.progress === 100 && (
            <Button
              onClick={() =>
                setRenderState({ rendering: false, progress: 0 })
              }
              variant="outline"
            >
              Reset
            </Button>
          )}
        </div>

        {/* Error Display */}
        {error && (
          <div className="rounded-md border border-red-500 bg-red-50 p-3 text-sm text-red-700 dark:bg-red-950/30 dark:text-red-400">
            <div className="font-semibold">Render Error</div>
            <div className="mt-1 font-mono text-xs">{error.message}</div>
            {error.details && (
              <pre className="mt-2 overflow-auto whitespace-pre-wrap text-[10px]">
                {JSON.stringify(error.details, null, 2)}
              </pre>
            )}
          </div>
        )}

        {/* Loading Indicator */}
        {loading && !renderState.rendering && (
          <div className="flex items-center justify-center gap-2 text-sm text-muted-foreground">
            <div className="h-4 w-4 animate-spin rounded-full border-2 border-current border-t-transparent" />
            <span>Initializing render...</span>
          </div>
        )}
      </CardContent>
    </Card>
  )
}
