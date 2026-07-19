/*
  ==============================================================================

    MatrixDisplay.cpp
    Created: Shmui-to-JUCE Audio Visualization Port

    Implementation of the LED matrix display component.

  ==============================================================================
*/

#include "MatrixDisplay.h"
#include <set>

namespace shmui
{

//==============================================================================
// Utility Functions

Frame createEmptyFrame(int rows, int cols)
{
    return Frame(rows, std::vector<float>(cols, 0.0f));
}

Frame createVUMeterFrame(int columns, const std::vector<float>& levels)
{
    // From matrix.tsx vu function
    const int rows = 7;
    Frame frame = createEmptyFrame(rows, columns);

    for (int col = 0; col < std::min(columns, static_cast<int>(levels.size())); ++col)
    {
        const float level = juce::jlimit(0.0f, 1.0f, levels[col]);
        const int height = static_cast<int>(level * rows);

        for (int row = 0; row < rows; ++row)
        {
            const int rowFromBottom = rows - 1 - row;

            if (rowFromBottom < height)
            {
                // Brightness gradient (top = brightest)
                float brightness;
                if (row < rows * 0.3f)
                    brightness = 1.0f;
                else if (row < rows * 0.6f)
                    brightness = 0.8f;
                else
                    brightness = 0.6f;

                frame[row][col] = brightness;
            }
        }
    }

    return frame;
}

//==============================================================================
// MatrixDisplay

MatrixDisplay::MatrixDisplay()
{
    currentFrame = createEmptyFrame(rows, cols);
    setOpaque(false);
}

MatrixDisplay::~MatrixDisplay()
{
    stopTimer();
}

void MatrixDisplay::setSize(int newRows, int newCols)
{
    rows = std::max(1, newRows);
    cols = std::max(1, newCols);
    currentFrame = createEmptyFrame(rows, cols);
    repaint();
}

void MatrixDisplay::setPattern(const Frame& pattern)
{
    // Stop any animation
    animationPlaying = false;
    stopTimer();
    vuLevels.clear();

    currentFrame = ensureFrameSize(pattern);
    repaint();
}

void MatrixDisplay::setFrames(const std::vector<Frame>& frames, float newFps, bool shouldLoop)
{
    animationFrames = frames;
    fps = newFps;
    loop = shouldLoop;
    frameIndex = 0;
    accumulator = 0.0f;
    vuLevels.clear();

    if (!animationFrames.empty())
    {
        currentFrame = ensureFrameSize(animationFrames[0]);
    }
    else
    {
        currentFrame = createEmptyFrame(rows, cols);
    }

    repaint();
}

void MatrixDisplay::setLevels(const std::vector<float>& levels)
{
    vuLevels = levels;
    currentFrame = createVUMeterFrame(cols, levels);
    repaint();
}

void MatrixDisplay::clear()
{
    animationPlaying = false;
    stopTimer();
    animationFrames.clear();
    vuLevels.clear();
    currentFrame = createEmptyFrame(rows, cols);
    repaint();
}

void MatrixDisplay::play()
{
    if (!animationFrames.empty())
    {
        animationPlaying = true;
        lastTime = juce::Time::currentTimeMillis();
        startTimerHz(60);
    }
}

void MatrixDisplay::stop()
{
    animationPlaying = false;
    stopTimer();
}

void MatrixDisplay::setFPS(float newFps)
{
    fps = std::max(0.1f, newFps);
}

void MatrixDisplay::setLoop(bool shouldLoop)
{
    loop = shouldLoop;
}

void MatrixDisplay::setLEDSize(float size)
{
    ledSize = std::max(1.0f, size);
    repaint();
}

void MatrixDisplay::setLEDGap(float gap)
{
    ledGap = std::max(0.0f, gap);
    repaint();
}

void MatrixDisplay::setOnColour(const juce::Colour& colour)
{
    onColour = colour;
    repaint();
}

void MatrixDisplay::setOffColour(const juce::Colour& colour)
{
    offColour = colour;
    repaint();
}

void MatrixDisplay::setBrightness(float newBrightness)
{
    brightness = juce::jlimit(0.0f, 1.0f, newBrightness);
    repaint();
}

void MatrixDisplay::paint(juce::Graphics& g)
{
    const float totalWidth = cols * (ledSize + ledGap) - ledGap;
    const float totalHeight = rows * (ledSize + ledGap) - ledGap;

    // Center the matrix
    const float startX = (getWidth() - totalWidth) / 2.0f;
    const float startY = (getHeight() - totalHeight) / 2.0f;

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const float value = (row < static_cast<int>(currentFrame.size()) &&
                                col < static_cast<int>(currentFrame[row].size()))
                               ? currentFrame[row][col] : 0.0f;

            const float x = startX + col * (ledSize + ledGap);
            const float y = startY + row * (ledSize + ledGap);

            const float opacity = juce::jlimit(0.0f, 1.0f, brightness * value);
            const bool isActive = opacity > 0.5f;
            const bool isOn = opacity > 0.05f;

            const float radius = (ledSize / 2.0f) * 0.9f;
            const float centerX = x + ledSize / 2.0f;
            const float centerY = y + ledSize / 2.0f;

            if (isOn)
            {
                // Glow effect for active LEDs
                if (isActive)
                {
                    g.setColour(onColour.withAlpha(opacity * 0.3f));
                    g.fillEllipse(centerX - radius * 1.4f, centerY - radius * 1.4f,
                                 radius * 2.8f, radius * 2.8f);
                }

                // LED body with gradient effect
                juce::ColourGradient gradient(
                    onColour.withAlpha(opacity),
                    centerX, centerY,
                    onColour.withAlpha(opacity * 0.6f),
                    centerX + radius, centerY + radius,
                    true);

                g.setGradientFill(gradient);
                g.fillEllipse(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f);
            }
            else
            {
                // Inactive LED
                g.setColour(offColour.withAlpha(0.1f));
                g.fillEllipse(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f);
            }
        }
    }
}

void MatrixDisplay::resized()
{
    repaint();
}

void MatrixDisplay::timerCallback()
{
    if (!animationPlaying || animationFrames.empty())
        return;

    const int64_t currentTime = juce::Time::currentTimeMillis();
    const float deltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f;
    lastTime = currentTime;

    accumulator += deltaTime;
    const float frameInterval = 1.0f / fps;

    while (accumulator >= frameInterval)
    {
        accumulator -= frameInterval;
        frameIndex++;

        if (frameIndex >= static_cast<int>(animationFrames.size()))
        {
            if (loop)
            {
                frameIndex = 0;
            }
            else
            {
                frameIndex = static_cast<int>(animationFrames.size()) - 1;
                animationPlaying = false;
                stopTimer();
            }
        }

        if (onFrame)
        {
            onFrame(frameIndex);
        }
    }

    currentFrame = ensureFrameSize(animationFrames[frameIndex]);
    repaint();
}

Frame MatrixDisplay::ensureFrameSize(const Frame& frame) const
{
    Frame result(rows, std::vector<float>(cols, 0.0f));

    for (int r = 0; r < rows; ++r)
    {
        const auto& row = (r < static_cast<int>(frame.size())) ? frame[r] : std::vector<float>();

        for (int c = 0; c < cols; ++c)
        {
            result[r][c] = (c < static_cast<int>(row.size())) ? row[c] : 0.0f;
        }
    }

    return result;
}

//==============================================================================
// MatrixAnimations

namespace MatrixAnimations
{

std::vector<Frame> createLoader()
{
    // From matrix.tsx loader
    std::vector<Frame> frames;
    const int size = 7;
    const int center = 3;
    const float radius = 2.5f;

    for (int frame = 0; frame < 12; ++frame)
    {
        Frame f = createEmptyFrame(size, size);

        for (int i = 0; i < 8; ++i)
        {
            const float angle = (static_cast<float>(frame) / 12.0f) * juce::MathConstants<float>::twoPi +
                               (static_cast<float>(i) / 8.0f) * juce::MathConstants<float>::twoPi;

            const int x = static_cast<int>(std::round(center + std::cos(angle) * radius));
            const int y = static_cast<int>(std::round(center + std::sin(angle) * radius));

            const float brightness = 1.0f - i / 10.0f;

            if (y >= 0 && y < size && x >= 0 && x < size)
            {
                f[y][x] = std::max(0.2f, brightness);
            }
        }

        frames.push_back(f);
    }

    return frames;
}

std::vector<Frame> createPulse()
{
    // From matrix.tsx pulse
    std::vector<Frame> frames;
    const int size = 7;
    const int center = 3;

    for (int frame = 0; frame < 16; ++frame)
    {
        Frame f = createEmptyFrame(size, size);

        const float phase = (static_cast<float>(frame) / 16.0f) * juce::MathConstants<float>::twoPi;
        const float intensity = (std::sin(phase) + 1.0f) / 2.0f;

        // Center point
        f[center][center] = 1.0f;

        // Expanding ring
        const int radius = static_cast<int>((1.0f - intensity) * 3.0f) + 1;

        for (int dy = -radius; dy <= radius; ++dy)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                const float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

                if (std::abs(dist - radius) < 0.7f)
                {
                    const int py = center + dy;
                    const int px = center + dx;

                    if (py >= 0 && py < size && px >= 0 && px < size)
                    {
                        f[py][px] = intensity * 0.6f;
                    }
                }
            }
        }

        frames.push_back(f);
    }

    return frames;
}

std::vector<Frame> createWave()
{
    // From matrix.tsx wave
    std::vector<Frame> frames;
    const int rows = 7;
    const int cols = 7;

    for (int frame = 0; frame < 24; ++frame)
    {
        Frame f = createEmptyFrame(rows, cols);

        const float phase = (static_cast<float>(frame) / 24.0f) * juce::MathConstants<float>::twoPi;

        for (int col = 0; col < cols; ++col)
        {
            const float colPhase = (static_cast<float>(col) / cols) * juce::MathConstants<float>::twoPi;
            const float height = std::sin(phase + colPhase) * 2.5f + 3.5f;
            const int row = static_cast<int>(height);

            if (row >= 0 && row < rows)
            {
                f[row][col] = 1.0f;

                const float frac = height - row;

                if (row > 0)
                    f[row - 1][col] = 1.0f - frac;

                if (row < rows - 1)
                    f[row + 1][col] = frac;
            }
        }

        frames.push_back(f);
    }

    return frames;
}

std::vector<Frame> createSnake()
{
    // From matrix.tsx snake
    std::vector<Frame> frames;
    const int rows = 7;
    const int cols = 7;

    // Generate snake path
    std::vector<std::pair<int, int>> path;
    int x = 0, y = 0;
    int dx = 1, dy = 0;

    std::set<std::pair<int, int>> visited;

    while (static_cast<int>(path.size()) < rows * cols)
    {
        path.push_back({y, x});
        visited.insert({y, x});

        int nextX = x + dx;
        int nextY = y + dy;

        if (nextX >= 0 && nextX < cols && nextY >= 0 && nextY < rows &&
            visited.find({nextY, nextX}) == visited.end())
        {
            x = nextX;
            y = nextY;
        }
        else
        {
            // Turn
            int newDx = -dy;
            int newDy = dx;
            dx = newDx;
            dy = newDy;

            nextX = x + dx;
            nextY = y + dy;

            if (nextX >= 0 && nextX < cols && nextY >= 0 && nextY < rows &&
                visited.find({nextY, nextX}) == visited.end())
            {
                x = nextX;
                y = nextY;
            }
            else
            {
                break;
            }
        }
    }

    // Generate frames
    const int snakeLength = 5;

    for (size_t frame = 0; frame < path.size(); ++frame)
    {
        Frame f = createEmptyFrame(rows, cols);

        for (int i = 0; i < snakeLength; ++i)
        {
            const int idx = static_cast<int>(frame) - i;

            if (idx >= 0 && idx < static_cast<int>(path.size()))
            {
                const auto& [py, px] = path[idx];
                const float brightness = 1.0f - static_cast<float>(i) / snakeLength;
                f[py][px] = brightness;
            }
        }

        frames.push_back(f);
    }

    return frames;
}

// Static storage for digits
static std::vector<Frame> s_digits;
static Frame s_chevronLeft;
static Frame s_chevronRight;
static bool s_initialized = false;

static void initializeSymbols()
{
    if (s_initialized)
        return;

    // Digit patterns from matrix.tsx (all 10 digits 0-9)
    s_digits = {
        // 0
        {{0, 1, 1, 1, 0},
         {1, 0, 0, 0, 1},
         {1, 0, 0, 0, 1},
         {1, 0, 0, 0, 1},
         {1, 0, 0, 0, 1},
         {1, 0, 0, 0, 1},
         {0, 1, 1, 1, 0}},
        // 1
        {{0, 0, 1, 0, 0},
         {0, 1, 1, 0, 0},
         {0, 0, 1, 0, 0},
         {0, 0, 1, 0, 0},
         {0, 0, 1, 0, 0},
         {0, 0, 1, 0, 0},
         {0, 1, 1, 1, 0}},
        // 2
        {{0, 1, 1, 1, 0},
         {1, 0, 0, 0, 1},
         {0, 0, 0, 0, 1},
         {0, 0, 1, 1, 0},
         {0, 1, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1, 1, 1, 1, 1}},
        // 3
        {{0, 1, 1, 1, 0},
         {1, 0, 0, 0, 1},
         {0, 0, 0, 0, 1},
         {0, 0, 1, 1, 0},
         {0, 0, 0, 0, 1},
         {1, 0, 0, 0, 1},
         {0, 1, 1, 1, 0}},
        // 4
        {{0, 0, 0, 1, 0},
         {0, 0, 1, 1, 0},
         {0, 1, 0, 1, 0},
         {1, 0, 0, 1, 0},
         {1, 1, 1, 1, 1},
         {0, 0, 0, 1, 0},
         {0, 0, 0, 1, 0}},
        // 5
        {{1, 1, 1, 1, 1},
         {1, 0, 0, 0, 0},
         {1, 1, 1, 1, 0},
         {0, 0, 0, 0, 1},
         {0, 0, 0, 0, 1},
         {1, 0, 0, 0, 1},
         {0, 1, 1, 1, 0}},
        // 6
        {{0, 0, 1, 1, 0},
         {0, 1, 0, 0, 0},
         {1, 0, 0, 0, 0},
         {1, 1, 1, 1, 0},
         {1, 0, 0, 0, 1},
         {1, 0, 0, 0, 1},
         {0, 1, 1, 1, 0}},
        // 7
        {{1, 1, 1, 1, 1},
         {0, 0, 0, 0, 1},
         {0, 0, 0, 1, 0},
         {0, 0, 1, 0, 0},
         {0, 1, 0, 0, 0},
         {0, 1, 0, 0, 0},
         {0, 1, 0, 0, 0}},
        // 8
        {{0, 1, 1, 1, 0},
         {1, 0, 0, 0, 1},
         {1, 0, 0, 0, 1},
         {0, 1, 1, 1, 0},
         {1, 0, 0, 0, 1},
         {1, 0, 0, 0, 1},
         {0, 1, 1, 1, 0}},
        // 9
        {{0, 1, 1, 1, 0},
         {1, 0, 0, 0, 1},
         {1, 0, 0, 0, 1},
         {0, 1, 1, 1, 1},
         {0, 0, 0, 0, 1},
         {0, 0, 0, 1, 0},
         {0, 1, 1, 0, 0}},
    };

    s_chevronLeft = {
        {0, 0, 0, 1, 0},
        {0, 0, 1, 0, 0},
        {0, 1, 0, 0, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0}};

    s_chevronRight = {
        {0, 1, 0, 0, 0},
        {0, 0, 1, 0, 0},
        {0, 0, 0, 1, 0},
        {0, 0, 1, 0, 0},
        {0, 1, 0, 0, 0}};

    s_initialized = true;
}

const std::vector<Frame>& getDigits()
{
    initializeSymbols();
    return s_digits;
}

const Frame& getChevronLeft()
{
    initializeSymbols();
    return s_chevronLeft;
}

const Frame& getChevronRight()
{
    initializeSymbols();
    return s_chevronRight;
}

} // namespace MatrixAnimations

} // namespace shmui
