/*
  ==============================================================================

    Icons.cpp
    Created: shmui Icon Library

    SVG path implementations for all icons.
    Paths are designed for a 24x24 viewbox and scaled to requested size.

  ==============================================================================
*/

#include "Icons.h"

namespace shmui
{
namespace Icons
{

namespace
{

//==============================================================================
// Path creation helpers - all paths designed for 24x24 viewbox

juce::Path createPlayPath()
{
    juce::Path p;
    p.addTriangle(6.0f, 4.0f, 6.0f, 20.0f, 20.0f, 12.0f);
    return p;
}

juce::Path createPausePath()
{
    juce::Path p;
    p.addRectangle(5.0f, 4.0f, 5.0f, 16.0f);
    p.addRectangle(14.0f, 4.0f, 5.0f, 16.0f);
    return p;
}

juce::Path createStopPath()
{
    juce::Path p;
    p.addRectangle(5.0f, 5.0f, 14.0f, 14.0f);
    return p;
}

juce::Path createRecordPath()
{
    juce::Path p;
    p.addEllipse(4.0f, 4.0f, 16.0f, 16.0f);
    return p;
}

juce::Path createFastForwardPath()
{
    juce::Path p;
    p.addTriangle(4.0f, 4.0f, 4.0f, 20.0f, 12.0f, 12.0f);
    p.addTriangle(12.0f, 4.0f, 12.0f, 20.0f, 20.0f, 12.0f);
    return p;
}

juce::Path createRewindPath()
{
    juce::Path p;
    p.addTriangle(20.0f, 4.0f, 20.0f, 20.0f, 12.0f, 12.0f);
    p.addTriangle(12.0f, 4.0f, 12.0f, 20.0f, 4.0f, 12.0f);
    return p;
}

juce::Path createSkipNextPath()
{
    juce::Path p;
    p.addTriangle(5.0f, 4.0f, 5.0f, 20.0f, 16.0f, 12.0f);
    p.addRectangle(17.0f, 4.0f, 3.0f, 16.0f);
    return p;
}

juce::Path createSkipPrevPath()
{
    juce::Path p;
    p.addRectangle(4.0f, 4.0f, 3.0f, 16.0f);
    p.addTriangle(19.0f, 4.0f, 19.0f, 20.0f, 8.0f, 12.0f);
    return p;
}

juce::Path createLoopPath()
{
    juce::Path p;
    // Circular arrows indicating loop
    p.addArc(4.0f, 6.0f, 16.0f, 12.0f, 0.0f, juce::MathConstants<float>::pi, true);
    p.addArc(4.0f, 6.0f, 16.0f, 12.0f, juce::MathConstants<float>::pi, juce::MathConstants<float>::twoPi, true);
    // Arrow heads
    p.addTriangle(18.0f, 6.0f, 22.0f, 6.0f, 20.0f, 2.0f);
    p.addTriangle(2.0f, 18.0f, 6.0f, 18.0f, 4.0f, 22.0f);
    return p;
}

juce::Path createVolumeHighPath()
{
    juce::Path p;
    // Speaker cone
    p.startNewSubPath(3.0f, 9.0f);
    p.lineTo(7.0f, 9.0f);
    p.lineTo(11.0f, 5.0f);
    p.lineTo(11.0f, 19.0f);
    p.lineTo(7.0f, 15.0f);
    p.lineTo(3.0f, 15.0f);
    p.closeSubPath();
    // Sound waves
    p.addArc(12.0f, 6.0f, 6.0f, 12.0f, -0.8f, 0.8f, true);
    p.addArc(14.0f, 3.0f, 8.0f, 18.0f, -1.0f, 1.0f, true);
    return p;
}

juce::Path createVolumeMidPath()
{
    juce::Path p;
    p.startNewSubPath(3.0f, 9.0f);
    p.lineTo(7.0f, 9.0f);
    p.lineTo(11.0f, 5.0f);
    p.lineTo(11.0f, 19.0f);
    p.lineTo(7.0f, 15.0f);
    p.lineTo(3.0f, 15.0f);
    p.closeSubPath();
    p.addArc(12.0f, 6.0f, 6.0f, 12.0f, -0.8f, 0.8f, true);
    return p;
}

juce::Path createVolumeLowPath()
{
    juce::Path p;
    p.startNewSubPath(6.0f, 9.0f);
    p.lineTo(10.0f, 9.0f);
    p.lineTo(14.0f, 5.0f);
    p.lineTo(14.0f, 19.0f);
    p.lineTo(10.0f, 15.0f);
    p.lineTo(6.0f, 15.0f);
    p.closeSubPath();
    return p;
}

juce::Path createVolumeMutePath()
{
    juce::Path p;
    p.startNewSubPath(3.0f, 9.0f);
    p.lineTo(7.0f, 9.0f);
    p.lineTo(11.0f, 5.0f);
    p.lineTo(11.0f, 19.0f);
    p.lineTo(7.0f, 15.0f);
    p.lineTo(3.0f, 15.0f);
    p.closeSubPath();
    // X mark
    p.startNewSubPath(14.0f, 9.0f);
    p.lineTo(20.0f, 15.0f);
    p.startNewSubPath(20.0f, 9.0f);
    p.lineTo(14.0f, 15.0f);
    return p;
}

juce::Path createMicrophonePath()
{
    juce::Path p;
    // Mic body
    p.addRoundedRectangle(8.0f, 2.0f, 8.0f, 12.0f, 4.0f);
    // Stand
    p.addArc(5.0f, 8.0f, 14.0f, 10.0f, 0.0f, juce::MathConstants<float>::pi, true);
    p.startNewSubPath(12.0f, 18.0f);
    p.lineTo(12.0f, 22.0f);
    p.startNewSubPath(8.0f, 22.0f);
    p.lineTo(16.0f, 22.0f);
    return p;
}

juce::Path createSpeakerPath()
{
    juce::Path p;
    p.addRoundedRectangle(4.0f, 2.0f, 16.0f, 20.0f, 2.0f);
    p.addEllipse(7.0f, 4.0f, 10.0f, 10.0f);
    p.addEllipse(9.0f, 16.0f, 6.0f, 4.0f);
    return p;
}

juce::Path createHeadphonesPath()
{
    juce::Path p;
    p.addArc(4.0f, 2.0f, 16.0f, 16.0f, juce::MathConstants<float>::pi, 0.0f, true);
    p.addRoundedRectangle(2.0f, 14.0f, 6.0f, 8.0f, 2.0f);
    p.addRoundedRectangle(16.0f, 14.0f, 6.0f, 8.0f, 2.0f);
    return p;
}

juce::Path createWaveformPath()
{
    juce::Path p;
    // Vertical bars representing waveform
    p.addRectangle(3.0f, 8.0f, 2.0f, 8.0f);
    p.addRectangle(7.0f, 4.0f, 2.0f, 16.0f);
    p.addRectangle(11.0f, 6.0f, 2.0f, 12.0f);
    p.addRectangle(15.0f, 3.0f, 2.0f, 18.0f);
    p.addRectangle(19.0f, 7.0f, 2.0f, 10.0f);
    return p;
}

juce::Path createSoloPath()
{
    juce::Path p;
    // S letter
    p.startNewSubPath(16.0f, 6.0f);
    p.cubicTo(16.0f, 4.0f, 14.0f, 4.0f, 12.0f, 4.0f);
    p.cubicTo(10.0f, 4.0f, 8.0f, 5.0f, 8.0f, 7.0f);
    p.cubicTo(8.0f, 9.0f, 10.0f, 10.0f, 12.0f, 11.0f);
    p.cubicTo(14.0f, 12.0f, 16.0f, 13.0f, 16.0f, 15.0f);
    p.cubicTo(16.0f, 17.0f, 14.0f, 18.0f, 12.0f, 18.0f);
    p.cubicTo(10.0f, 18.0f, 8.0f, 17.0f, 8.0f, 15.0f);
    return p;
}

juce::Path createMutePath()
{
    juce::Path p;
    // M letter
    p.startNewSubPath(4.0f, 18.0f);
    p.lineTo(4.0f, 6.0f);
    p.lineTo(9.0f, 14.0f);
    p.lineTo(14.0f, 6.0f);
    p.lineTo(14.0f, 18.0f);
    return p;
}

juce::Path createBypassPath()
{
    juce::Path p;
    // Circle with diagonal line
    p.addEllipse(4.0f, 4.0f, 16.0f, 16.0f);
    p.startNewSubPath(7.0f, 7.0f);
    p.lineTo(17.0f, 17.0f);
    return p;
}

juce::Path createPanPath()
{
    juce::Path p;
    // Horizontal slider track
    p.addRoundedRectangle(2.0f, 10.0f, 20.0f, 4.0f, 2.0f);
    // Center knob
    p.addEllipse(9.0f, 7.0f, 6.0f, 10.0f);
    return p;
}

juce::Path createFaderPath()
{
    juce::Path p;
    // Vertical track
    p.addRoundedRectangle(10.0f, 2.0f, 4.0f, 20.0f, 2.0f);
    // Horizontal handle
    p.addRoundedRectangle(6.0f, 9.0f, 12.0f, 6.0f, 2.0f);
    return p;
}

juce::Path createFolderPath()
{
    juce::Path p;
    p.startNewSubPath(2.0f, 8.0f);
    p.lineTo(2.0f, 18.0f);
    p.lineTo(22.0f, 18.0f);
    p.lineTo(22.0f, 8.0f);
    p.closeSubPath();
    p.startNewSubPath(2.0f, 8.0f);
    p.lineTo(2.0f, 6.0f);
    p.lineTo(10.0f, 6.0f);
    p.lineTo(12.0f, 8.0f);
    p.closeSubPath();
    return p;
}

juce::Path createFolderOpenPath()
{
    juce::Path p;
    p.startNewSubPath(2.0f, 18.0f);
    p.lineTo(5.0f, 10.0f);
    p.lineTo(22.0f, 10.0f);
    p.lineTo(19.0f, 18.0f);
    p.closeSubPath();
    p.startNewSubPath(2.0f, 10.0f);
    p.lineTo(2.0f, 6.0f);
    p.lineTo(10.0f, 6.0f);
    p.lineTo(12.0f, 8.0f);
    p.lineTo(22.0f, 8.0f);
    p.lineTo(22.0f, 10.0f);
    return p;
}

juce::Path createFilePath()
{
    juce::Path p;
    p.startNewSubPath(4.0f, 2.0f);
    p.lineTo(14.0f, 2.0f);
    p.lineTo(20.0f, 8.0f);
    p.lineTo(20.0f, 22.0f);
    p.lineTo(4.0f, 22.0f);
    p.closeSubPath();
    // Fold corner
    p.startNewSubPath(14.0f, 2.0f);
    p.lineTo(14.0f, 8.0f);
    p.lineTo(20.0f, 8.0f);
    return p;
}

juce::Path createFileAudioPath()
{
    juce::Path p;
    p.startNewSubPath(4.0f, 2.0f);
    p.lineTo(14.0f, 2.0f);
    p.lineTo(20.0f, 8.0f);
    p.lineTo(20.0f, 22.0f);
    p.lineTo(4.0f, 22.0f);
    p.closeSubPath();
    p.startNewSubPath(14.0f, 2.0f);
    p.lineTo(14.0f, 8.0f);
    p.lineTo(20.0f, 8.0f);
    // Audio symbol
    p.addRectangle(8.0f, 12.0f, 2.0f, 6.0f);
    p.addRectangle(11.0f, 14.0f, 2.0f, 4.0f);
    p.addRectangle(14.0f, 11.0f, 2.0f, 7.0f);
    return p;
}

juce::Path createImportPath()
{
    juce::Path p;
    // Arrow pointing in
    p.startNewSubPath(12.0f, 3.0f);
    p.lineTo(12.0f, 15.0f);
    p.startNewSubPath(8.0f, 11.0f);
    p.lineTo(12.0f, 15.0f);
    p.lineTo(16.0f, 11.0f);
    // Box
    p.startNewSubPath(4.0f, 12.0f);
    p.lineTo(4.0f, 20.0f);
    p.lineTo(20.0f, 20.0f);
    p.lineTo(20.0f, 12.0f);
    return p;
}

juce::Path createExportPath()
{
    juce::Path p;
    // Arrow pointing out
    p.startNewSubPath(12.0f, 15.0f);
    p.lineTo(12.0f, 3.0f);
    p.startNewSubPath(8.0f, 7.0f);
    p.lineTo(12.0f, 3.0f);
    p.lineTo(16.0f, 7.0f);
    // Box
    p.startNewSubPath(4.0f, 12.0f);
    p.lineTo(4.0f, 20.0f);
    p.lineTo(20.0f, 20.0f);
    p.lineTo(20.0f, 12.0f);
    return p;
}

juce::Path createSavePath()
{
    juce::Path p;
    // Floppy disk shape
    p.addRoundedRectangle(3.0f, 3.0f, 18.0f, 18.0f, 2.0f);
    p.addRectangle(7.0f, 3.0f, 10.0f, 8.0f);
    p.addRectangle(6.0f, 14.0f, 12.0f, 7.0f);
    return p;
}

juce::Path createLoadPath()
{
    juce::Path p;
    // Folder with arrow
    createFolderPath();
    p.startNewSubPath(12.0f, 10.0f);
    p.lineTo(12.0f, 18.0f);
    p.startNewSubPath(9.0f, 14.0f);
    p.lineTo(12.0f, 18.0f);
    p.lineTo(15.0f, 14.0f);
    return p;
}

juce::Path createCutPath()
{
    juce::Path p;
    // Scissors
    p.addEllipse(4.0f, 14.0f, 6.0f, 6.0f);
    p.addEllipse(14.0f, 14.0f, 6.0f, 6.0f);
    p.startNewSubPath(9.0f, 15.0f);
    p.lineTo(15.0f, 4.0f);
    p.startNewSubPath(15.0f, 15.0f);
    p.lineTo(9.0f, 4.0f);
    return p;
}

juce::Path createCopyPath()
{
    juce::Path p;
    // Two overlapping rectangles
    p.addRoundedRectangle(8.0f, 8.0f, 12.0f, 14.0f, 2.0f);
    p.addRoundedRectangle(4.0f, 4.0f, 12.0f, 14.0f, 2.0f);
    return p;
}

juce::Path createPastePath()
{
    juce::Path p;
    // Clipboard
    p.addRoundedRectangle(4.0f, 4.0f, 16.0f, 18.0f, 2.0f);
    p.addRoundedRectangle(8.0f, 2.0f, 8.0f, 4.0f, 1.0f);
    // Lines
    p.startNewSubPath(8.0f, 10.0f);
    p.lineTo(16.0f, 10.0f);
    p.startNewSubPath(8.0f, 14.0f);
    p.lineTo(16.0f, 14.0f);
    return p;
}

juce::Path createUndoPath()
{
    juce::Path p;
    // Curved arrow left
    p.addArc(6.0f, 6.0f, 14.0f, 14.0f, juce::MathConstants<float>::pi * 0.5f, juce::MathConstants<float>::pi * 1.5f, true);
    p.addTriangle(4.0f, 8.0f, 10.0f, 4.0f, 10.0f, 12.0f);
    return p;
}

juce::Path createRedoPath()
{
    juce::Path p;
    // Curved arrow right
    p.addArc(4.0f, 6.0f, 14.0f, 14.0f, juce::MathConstants<float>::pi * 1.5f, juce::MathConstants<float>::pi * 0.5f, true);
    p.addTriangle(20.0f, 8.0f, 14.0f, 4.0f, 14.0f, 12.0f);
    return p;
}

juce::Path createDeletePath()
{
    juce::Path p;
    // Trash can
    p.addRoundedRectangle(5.0f, 6.0f, 14.0f, 16.0f, 2.0f);
    p.startNewSubPath(3.0f, 6.0f);
    p.lineTo(21.0f, 6.0f);
    p.addRoundedRectangle(8.0f, 3.0f, 8.0f, 3.0f, 1.0f);
    // Lines
    p.startNewSubPath(9.0f, 10.0f);
    p.lineTo(9.0f, 18.0f);
    p.startNewSubPath(12.0f, 10.0f);
    p.lineTo(12.0f, 18.0f);
    p.startNewSubPath(15.0f, 10.0f);
    p.lineTo(15.0f, 18.0f);
    return p;
}

juce::Path createDuplicatePath()
{
    juce::Path p;
    // Two overlapping rectangles with plus
    p.addRoundedRectangle(8.0f, 8.0f, 12.0f, 14.0f, 2.0f);
    p.addRoundedRectangle(4.0f, 4.0f, 12.0f, 14.0f, 2.0f);
    // Plus sign
    p.startNewSubPath(10.0f, 8.0f);
    p.lineTo(10.0f, 14.0f);
    p.startNewSubPath(7.0f, 11.0f);
    p.lineTo(13.0f, 11.0f);
    return p;
}

juce::Path createEditPath()
{
    juce::Path p;
    // Pencil
    p.startNewSubPath(16.0f, 3.0f);
    p.lineTo(21.0f, 8.0f);
    p.lineTo(8.0f, 21.0f);
    p.lineTo(3.0f, 21.0f);
    p.lineTo(3.0f, 16.0f);
    p.closeSubPath();
    p.startNewSubPath(14.0f, 5.0f);
    p.lineTo(19.0f, 10.0f);
    return p;
}

juce::Path createMenuPath()
{
    juce::Path p;
    p.addRectangle(3.0f, 5.0f, 18.0f, 2.0f);
    p.addRectangle(3.0f, 11.0f, 18.0f, 2.0f);
    p.addRectangle(3.0f, 17.0f, 18.0f, 2.0f);
    return p;
}

juce::Path createClosePath()
{
    juce::Path p;
    p.startNewSubPath(6.0f, 6.0f);
    p.lineTo(18.0f, 18.0f);
    p.startNewSubPath(18.0f, 6.0f);
    p.lineTo(6.0f, 18.0f);
    return p;
}

juce::Path createMinimizePath()
{
    juce::Path p;
    p.addRectangle(4.0f, 11.0f, 16.0f, 2.0f);
    return p;
}

juce::Path createMaximizePath()
{
    juce::Path p;
    p.addRectangle(4.0f, 4.0f, 16.0f, 16.0f);
    return p;
}

juce::Path createSettingsPath()
{
    juce::Path p;
    // Gear
    p.addEllipse(8.0f, 8.0f, 8.0f, 8.0f);
    for (int i = 0; i < 8; ++i)
    {
        float angle = i * juce::MathConstants<float>::pi / 4.0f;
        float cx = 12.0f + std::cos(angle) * 9.0f;
        float cy = 12.0f + std::sin(angle) * 9.0f;
        p.addRectangle(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
    }
    return p;
}

juce::Path createSearchPath()
{
    juce::Path p;
    p.addEllipse(4.0f, 4.0f, 12.0f, 12.0f);
    p.startNewSubPath(14.0f, 14.0f);
    p.lineTo(20.0f, 20.0f);
    return p;
}

juce::Path createFilterPath()
{
    juce::Path p;
    p.startNewSubPath(3.0f, 4.0f);
    p.lineTo(21.0f, 4.0f);
    p.lineTo(14.0f, 12.0f);
    p.lineTo(14.0f, 20.0f);
    p.lineTo(10.0f, 18.0f);
    p.lineTo(10.0f, 12.0f);
    p.closeSubPath();
    return p;
}

juce::Path createPlusPath()
{
    juce::Path p;
    p.startNewSubPath(12.0f, 4.0f);
    p.lineTo(12.0f, 20.0f);
    p.startNewSubPath(4.0f, 12.0f);
    p.lineTo(20.0f, 12.0f);
    return p;
}

juce::Path createMinusPath()
{
    juce::Path p;
    p.startNewSubPath(4.0f, 12.0f);
    p.lineTo(20.0f, 12.0f);
    return p;
}

juce::Path createChevronUpPath()
{
    juce::Path p;
    p.startNewSubPath(6.0f, 15.0f);
    p.lineTo(12.0f, 9.0f);
    p.lineTo(18.0f, 15.0f);
    return p;
}

juce::Path createChevronDownPath()
{
    juce::Path p;
    p.startNewSubPath(6.0f, 9.0f);
    p.lineTo(12.0f, 15.0f);
    p.lineTo(18.0f, 9.0f);
    return p;
}

juce::Path createChevronLeftPath()
{
    juce::Path p;
    p.startNewSubPath(15.0f, 6.0f);
    p.lineTo(9.0f, 12.0f);
    p.lineTo(15.0f, 18.0f);
    return p;
}

juce::Path createChevronRightPath()
{
    juce::Path p;
    p.startNewSubPath(9.0f, 6.0f);
    p.lineTo(15.0f, 12.0f);
    p.lineTo(9.0f, 18.0f);
    return p;
}

juce::Path createArrowUpPath()
{
    juce::Path p;
    p.startNewSubPath(12.0f, 4.0f);
    p.lineTo(12.0f, 20.0f);
    p.startNewSubPath(6.0f, 10.0f);
    p.lineTo(12.0f, 4.0f);
    p.lineTo(18.0f, 10.0f);
    return p;
}

juce::Path createArrowDownPath()
{
    juce::Path p;
    p.startNewSubPath(12.0f, 20.0f);
    p.lineTo(12.0f, 4.0f);
    p.startNewSubPath(6.0f, 14.0f);
    p.lineTo(12.0f, 20.0f);
    p.lineTo(18.0f, 14.0f);
    return p;
}

juce::Path createArrowLeftPath()
{
    juce::Path p;
    p.startNewSubPath(4.0f, 12.0f);
    p.lineTo(20.0f, 12.0f);
    p.startNewSubPath(10.0f, 6.0f);
    p.lineTo(4.0f, 12.0f);
    p.lineTo(10.0f, 18.0f);
    return p;
}

juce::Path createArrowRightPath()
{
    juce::Path p;
    p.startNewSubPath(20.0f, 12.0f);
    p.lineTo(4.0f, 12.0f);
    p.startNewSubPath(14.0f, 6.0f);
    p.lineTo(20.0f, 12.0f);
    p.lineTo(14.0f, 18.0f);
    return p;
}

juce::Path createCheckPath()
{
    juce::Path p;
    p.startNewSubPath(5.0f, 12.0f);
    p.lineTo(10.0f, 17.0f);
    p.lineTo(19.0f, 7.0f);
    return p;
}

juce::Path createXPath()
{
    juce::Path p;
    p.startNewSubPath(6.0f, 6.0f);
    p.lineTo(18.0f, 18.0f);
    p.startNewSubPath(18.0f, 6.0f);
    p.lineTo(6.0f, 18.0f);
    return p;
}

juce::Path createWarningPath()
{
    juce::Path p;
    // Triangle
    p.startNewSubPath(12.0f, 2.0f);
    p.lineTo(22.0f, 20.0f);
    p.lineTo(2.0f, 20.0f);
    p.closeSubPath();
    // Exclamation
    p.addRectangle(11.0f, 8.0f, 2.0f, 6.0f);
    p.addEllipse(11.0f, 16.0f, 2.0f, 2.0f);
    return p;
}

juce::Path createInfoPath()
{
    juce::Path p;
    p.addEllipse(4.0f, 4.0f, 16.0f, 16.0f);
    p.addRectangle(11.0f, 10.0f, 2.0f, 7.0f);
    p.addEllipse(11.0f, 7.0f, 2.0f, 2.0f);
    return p;
}

juce::Path createErrorPath()
{
    juce::Path p;
    p.addEllipse(4.0f, 4.0f, 16.0f, 16.0f);
    p.startNewSubPath(9.0f, 9.0f);
    p.lineTo(15.0f, 15.0f);
    p.startNewSubPath(15.0f, 9.0f);
    p.lineTo(9.0f, 15.0f);
    return p;
}

juce::Path createLoadingPath()
{
    juce::Path p;
    // Circular loading indicator (partial circle)
    p.addArc(4.0f, 4.0f, 16.0f, 16.0f, 0.0f, juce::MathConstants<float>::pi * 1.5f, true);
    return p;
}

juce::Path createClockPath()
{
    juce::Path p;
    p.addEllipse(4.0f, 4.0f, 16.0f, 16.0f);
    // Clock hands
    p.startNewSubPath(12.0f, 12.0f);
    p.lineTo(12.0f, 7.0f);
    p.startNewSubPath(12.0f, 12.0f);
    p.lineTo(16.0f, 12.0f);
    return p;
}

} // anonymous namespace

//==============================================================================
juce::Path getIcon(IconType type, float size)
{
    juce::Path path;

    switch (type)
    {
        case IconType::Play:         path = createPlayPath(); break;
        case IconType::Pause:        path = createPausePath(); break;
        case IconType::Stop:         path = createStopPath(); break;
        case IconType::Record:       path = createRecordPath(); break;
        case IconType::FastForward:  path = createFastForwardPath(); break;
        case IconType::Rewind:       path = createRewindPath(); break;
        case IconType::SkipNext:     path = createSkipNextPath(); break;
        case IconType::SkipPrev:     path = createSkipPrevPath(); break;
        case IconType::Loop:         path = createLoopPath(); break;

        case IconType::VolumeHigh:   path = createVolumeHighPath(); break;
        case IconType::VolumeMid:    path = createVolumeMidPath(); break;
        case IconType::VolumeLow:    path = createVolumeLowPath(); break;
        case IconType::VolumeMute:   path = createVolumeMutePath(); break;
        case IconType::Microphone:   path = createMicrophonePath(); break;
        case IconType::Speaker:      path = createSpeakerPath(); break;
        case IconType::Headphones:   path = createHeadphonesPath(); break;
        case IconType::Waveform:     path = createWaveformPath(); break;

        case IconType::Solo:         path = createSoloPath(); break;
        case IconType::Mute:         path = createMutePath(); break;
        case IconType::Bypass:       path = createBypassPath(); break;
        case IconType::Pan:          path = createPanPath(); break;
        case IconType::Fader:        path = createFaderPath(); break;

        case IconType::Folder:       path = createFolderPath(); break;
        case IconType::FolderOpen:   path = createFolderOpenPath(); break;
        case IconType::File:         path = createFilePath(); break;
        case IconType::FileAudio:    path = createFileAudioPath(); break;
        case IconType::Import:       path = createImportPath(); break;
        case IconType::Export:       path = createExportPath(); break;
        case IconType::Save:         path = createSavePath(); break;
        case IconType::Load:         path = createLoadPath(); break;

        case IconType::Cut:          path = createCutPath(); break;
        case IconType::Copy:         path = createCopyPath(); break;
        case IconType::Paste:        path = createPastePath(); break;
        case IconType::Undo:         path = createUndoPath(); break;
        case IconType::Redo:         path = createRedoPath(); break;
        case IconType::Delete:       path = createDeletePath(); break;
        case IconType::Duplicate:    path = createDuplicatePath(); break;
        case IconType::Edit:         path = createEditPath(); break;

        case IconType::Menu:         path = createMenuPath(); break;
        case IconType::Close:        path = createClosePath(); break;
        case IconType::Minimize:     path = createMinimizePath(); break;
        case IconType::Maximize:     path = createMaximizePath(); break;
        case IconType::Settings:     path = createSettingsPath(); break;
        case IconType::Search:       path = createSearchPath(); break;
        case IconType::Filter:       path = createFilterPath(); break;
        case IconType::Plus:         path = createPlusPath(); break;
        case IconType::Minus:        path = createMinusPath(); break;

        case IconType::ChevronUp:    path = createChevronUpPath(); break;
        case IconType::ChevronDown:  path = createChevronDownPath(); break;
        case IconType::ChevronLeft:  path = createChevronLeftPath(); break;
        case IconType::ChevronRight: path = createChevronRightPath(); break;
        case IconType::ArrowUp:      path = createArrowUpPath(); break;
        case IconType::ArrowDown:    path = createArrowDownPath(); break;
        case IconType::ArrowLeft:    path = createArrowLeftPath(); break;
        case IconType::ArrowRight:   path = createArrowRightPath(); break;

        case IconType::Check:        path = createCheckPath(); break;
        case IconType::X:            path = createXPath(); break;
        case IconType::Warning:      path = createWarningPath(); break;
        case IconType::Info:         path = createInfoPath(); break;
        case IconType::Error:        path = createErrorPath(); break;
        case IconType::Loading:      path = createLoadingPath(); break;
        case IconType::Clock:        path = createClockPath(); break;

        default: break;
    }

    // Scale from 24x24 viewbox to requested size
    if (size != 24.0f && !path.isEmpty())
    {
        const float scale = size / 24.0f;
        path.applyTransform(juce::AffineTransform::scale(scale));
    }

    return path;
}

void drawIcon(juce::Graphics& g,
              IconType type,
              juce::Rectangle<float> bounds,
              juce::Colour colour,
              float strokeWidth)
{
    juce::Path path = getIcon(type, std::min(bounds.getWidth(), bounds.getHeight()));

    // Center the path in bounds
    auto pathBounds = path.getBounds();
    float offsetX = bounds.getX() + (bounds.getWidth() - pathBounds.getWidth()) * 0.5f - pathBounds.getX();
    float offsetY = bounds.getY() + (bounds.getHeight() - pathBounds.getHeight()) * 0.5f - pathBounds.getY();
    path.applyTransform(juce::AffineTransform::translation(offsetX, offsetY));

    g.setColour(colour);

    if (strokeWidth > 0.0f)
    {
        g.strokePath(path, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        g.fillPath(path);
    }
}

juce::String getIconName(IconType type)
{
    switch (type)
    {
        case IconType::Play:         return "Play";
        case IconType::Pause:        return "Pause";
        case IconType::Stop:         return "Stop";
        case IconType::Record:       return "Record";
        case IconType::FastForward:  return "Fast Forward";
        case IconType::Rewind:       return "Rewind";
        case IconType::SkipNext:     return "Skip Next";
        case IconType::SkipPrev:     return "Skip Previous";
        case IconType::Loop:         return "Loop";

        case IconType::VolumeHigh:   return "Volume High";
        case IconType::VolumeMid:    return "Volume Mid";
        case IconType::VolumeLow:    return "Volume Low";
        case IconType::VolumeMute:   return "Volume Mute";
        case IconType::Microphone:   return "Microphone";
        case IconType::Speaker:      return "Speaker";
        case IconType::Headphones:   return "Headphones";
        case IconType::Waveform:     return "Waveform";

        case IconType::Solo:         return "Solo";
        case IconType::Mute:         return "Mute";
        case IconType::Bypass:       return "Bypass";
        case IconType::Pan:          return "Pan";
        case IconType::Fader:        return "Fader";

        case IconType::Folder:       return "Folder";
        case IconType::FolderOpen:   return "Folder Open";
        case IconType::File:         return "File";
        case IconType::FileAudio:    return "Audio File";
        case IconType::Import:       return "Import";
        case IconType::Export:       return "Export";
        case IconType::Save:         return "Save";
        case IconType::Load:         return "Load";

        case IconType::Cut:          return "Cut";
        case IconType::Copy:         return "Copy";
        case IconType::Paste:        return "Paste";
        case IconType::Undo:         return "Undo";
        case IconType::Redo:         return "Redo";
        case IconType::Delete:       return "Delete";
        case IconType::Duplicate:    return "Duplicate";
        case IconType::Edit:         return "Edit";

        case IconType::Menu:         return "Menu";
        case IconType::Close:        return "Close";
        case IconType::Minimize:     return "Minimize";
        case IconType::Maximize:     return "Maximize";
        case IconType::Settings:     return "Settings";
        case IconType::Search:       return "Search";
        case IconType::Filter:       return "Filter";
        case IconType::Plus:         return "Plus";
        case IconType::Minus:        return "Minus";

        case IconType::ChevronUp:    return "Chevron Up";
        case IconType::ChevronDown:  return "Chevron Down";
        case IconType::ChevronLeft:  return "Chevron Left";
        case IconType::ChevronRight: return "Chevron Right";
        case IconType::ArrowUp:      return "Arrow Up";
        case IconType::ArrowDown:    return "Arrow Down";
        case IconType::ArrowLeft:    return "Arrow Left";
        case IconType::ArrowRight:   return "Arrow Right";

        case IconType::Check:        return "Check";
        case IconType::X:            return "X";
        case IconType::Warning:      return "Warning";
        case IconType::Info:         return "Info";
        case IconType::Error:        return "Error";
        case IconType::Loading:      return "Loading";
        case IconType::Clock:        return "Clock";

        default:                     return "Unknown";
    }
}

} // namespace Icons
} // namespace shmui
