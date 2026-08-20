/*
  ==============================================================================

    MatrixDisplay.cpp
    Created: Shmui-to-JUCE Audio Visualization Port

    Implementation of the LED matrix display component.

  ==============================================================================
*/

#include "MatrixDisplay.h"
#include <cmath>
#include <set>
namespace shmui {

//==============================================================================
// Utility Functions

Frame createEmptyFrame(int requestedRows, int requestedCols) {
  const int safeRows = juce::jlimit(1, MatrixDisplay::kMaxRows, requestedRows);
  const int safeCols = juce::jlimit(1, MatrixDisplay::kMaxColumns, requestedCols);
  return Frame(static_cast<std::size_t>(safeRows),
               std::vector<float>(static_cast<std::size_t>(safeCols), 0.0f));
}

Frame createVUMeterFrame(int columns, const std::vector<float>& levels) {
  // From matrix.tsx vu function
  const int safeColumns = juce::jlimit(1, MatrixDisplay::kMaxColumns, columns);
  const int rows = 7;
  Frame frame = createEmptyFrame(rows, safeColumns);

  for (int col = 0; col < std::min(safeColumns, static_cast<int>(levels.size())); ++col) {
    const float rawLevel = levels[static_cast<std::size_t>(col)];
    const float level = std::isfinite(rawLevel) ? juce::jlimit(0.0f, 1.0f, rawLevel) : 0.0f;
    const int height = static_cast<int>(level * rows);

    for (int row = 0; row < rows; ++row) {
      const int rowFromBottom = rows - 1 - row;

      if (rowFromBottom < height) {
        // Brightness gradient (top = brightest)
        float brightness;
        if (row < rows * 0.3f)
          brightness = 1.0f;
        else if (row < rows * 0.6f)
          brightness = 0.8f;
        else
          brightness = 0.6f;

        frame[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = brightness;
      }
    }
  }

  return frame;
}

MatrixDisplay::MatrixDisplay() {
  setOpaque(false);
  const auto& theme = defaultTheme();
  onColour = theme.fg;
  offColour = theme.fgMuted.withAlpha(0.5f);
  currentFrame = createEmptyFrame(rows, cols);
  addDefaultThemeListener(this);
}

MatrixDisplay::~MatrixDisplay() {
  removeDefaultThemeListener(this);
  stopTimer();
}

//==============================================================================
// MatrixDisplay

void MatrixDisplay::setSize(int newRows, int newCols) {
  if (!requireMessageThread())
    return;

  rows = juce::jlimit(1, kMaxRows, newRows);
  cols = juce::jlimit(1, kMaxColumns, newCols);
  currentFrame = createEmptyFrame(rows, cols);
  animationFrames.clear();
  frameIndex = 0;
  animationPlaying = false;
  stopTimer();
  repaint();
}
void MatrixDisplay::setPattern(const Frame& pattern) {
  if (!requireMessageThread())
    return;

  // Stop any animation
  animationPlaying = false;
  stopTimer();
  animationFrames.clear();
  vuLevels.clear();

  currentFrame = ensureFrameSize(pattern);
  repaint();
}
void MatrixDisplay::setFrames(const std::vector<Frame>& frames, float newFps, bool shouldLoop) {
  if (!requireMessageThread())
    return;

  animationFrames.clear();
  const auto count = std::min(frames.size(), static_cast<std::size_t>(kMaxFrames));
  animationFrames.reserve(count);
  for (std::size_t i = 0; i < count; ++i)
    animationFrames.push_back(ensureFrameSize(frames[i]));

  fps = std::isfinite(newFps) ? juce::jlimit(0.1f, 240.0f, newFps) : 12.0f;
  loop = shouldLoop;
  frameIndex = 0;
  accumulator = 0.0f;
  animationPlaying = false;
  vuLevels.clear();

  if (!animationFrames.empty())
    currentFrame = animationFrames.front();
  else
    currentFrame = createEmptyFrame(rows, cols);

  stopTimer();
  repaint();
}
void MatrixDisplay::setLevels(const std::vector<float>& levels) {
  if (!requireMessageThread())
    return;

  vuLevels.assign(levels.begin(),
                  levels.begin() + std::min(levels.size(), static_cast<std::size_t>(kMaxColumns)));
  currentFrame = createVUMeterFrame(cols, vuLevels);
  repaint();
}
void MatrixDisplay::clear() {
  if (!requireMessageThread())
    return;

  animationPlaying = false;
  stopTimer();
  animationFrames.clear();
  vuLevels.clear();
  currentFrame = createEmptyFrame(rows, cols);
  frameIndex = 0;
  accumulator = 0.0f;
  repaint();
}
void MatrixDisplay::play() {
  if (!requireMessageThread())
    return;

  if (!animationFrames.empty()) {
    animationPlaying = true;
    lastTime = juce::Time::currentTimeMillis();
    startTimerHz(60);
  }
}

void MatrixDisplay::stop() {
  if (!requireMessageThread())
    return;

  animationPlaying = false;
  stopTimer();
}

void MatrixDisplay::setFPS(float newFps) {
  if (!requireMessageThread())
    return;

  fps = std::isfinite(newFps) ? juce::jlimit(0.1f, 240.0f, newFps) : 12.0f;
}

void MatrixDisplay::setLoop(bool shouldLoop) {
  if (!requireMessageThread())
    return;

  loop = shouldLoop;
}

void MatrixDisplay::setLEDSize(float size) {
  if (!requireMessageThread())
    return;

  ledSize = std::isfinite(size) ? juce::jlimit(1.0f, 256.0f, size) : 1.0f;
  repaint();
}

void MatrixDisplay::setLEDGap(float gap) {
  if (!requireMessageThread())
    return;

  ledGap = std::isfinite(gap) ? juce::jlimit(0.0f, 256.0f, gap) : 0.0f;
  repaint();
}

void MatrixDisplay::setOnColour(const juce::Colour& colour) {
  if (!requireMessageThread())
    return;

  onColour = colour;
  customOnColour = true;
  repaint();
}

void MatrixDisplay::setOffColour(const juce::Colour& colour) {
  if (!requireMessageThread())
    return;

  offColour = colour;
  customOffColour = true;
  repaint();
}

void MatrixDisplay::setBrightness(float newBrightness) {
  if (!requireMessageThread())
    return;

  brightness = std::isfinite(newBrightness) ? juce::jlimit(0.0f, 1.0f, newBrightness) : 0.0f;
  repaint();
}

void MatrixDisplay::paint(juce::Graphics& g) {
  const float totalWidth = cols * (ledSize + ledGap) - ledGap;
  const float totalHeight = rows * (ledSize + ledGap) - ledGap;

  // Center the matrix
  const float startX = (getWidth() - totalWidth) / 2.0f;
  const float startY = (getHeight() - totalHeight) / 2.0f;

  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const float value = (row < static_cast<int>(currentFrame.size()) &&
                           col < static_cast<int>(currentFrame[row].size()))
                              ? currentFrame[row][col]
                              : 0.0f;

      const float x = startX + col * (ledSize + ledGap);
      const float y = startY + row * (ledSize + ledGap);

      const float opacity = juce::jlimit(0.0f, 1.0f, brightness * value);
      const bool isActive = opacity > 0.5f;
      const bool isOn = opacity > 0.05f;

      const float radius = (ledSize / 2.0f) * 0.9f;
      const float centerX = x + ledSize / 2.0f;
      const float centerY = y + ledSize / 2.0f;

      if (isOn) {
        // Glow effect for active LEDs
        if (isActive) {
          g.setColour(onColour.withAlpha(opacity * 0.3f));
          g.fillEllipse(centerX - radius * 1.4f, centerY - radius * 1.4f, radius * 2.8f,
                        radius * 2.8f);
        }

        // LED body with gradient effect
        juce::ColourGradient gradient(onColour.withAlpha(opacity), centerX, centerY,
                                      onColour.withAlpha(opacity * 0.6f), centerX + radius,
                                      centerY + radius, true);

        g.setGradientFill(gradient);
        g.fillEllipse(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f);
      } else {
        // Inactive LED
        g.setColour(offColour.withAlpha(0.1f));
        g.fillEllipse(centerX - radius, centerY - radius, radius * 2.0f, radius * 2.0f);
      }
    }
  }
}

void MatrixDisplay::defaultThemeChanged(const ShmuiTheme& theme) {
  if (!requireMessageThread())
    return;

  if (!customOnColour)
    onColour = theme.fg;
  if (!customOffColour)
    offColour = theme.fgMuted.withAlpha(0.5f);
  repaint();
}

void MatrixDisplay::resized() {
  if (requireMessageThread())
    repaint();
}

void MatrixDisplay::timerCallback() {
  if (!requireMessageThread())
    return;
  if (!animationPlaying || animationFrames.empty())
    return;

  const int64_t currentTime = juce::Time::currentTimeMillis();
  const float deltaTime =
      juce::jlimit(0.0f, 0.25f, static_cast<float>(currentTime - lastTime) / 1000.0f);
  lastTime = currentTime;

  accumulator += deltaTime;
  const float frameInterval = 1.0f / juce::jmax(0.1f, fps);

  while (accumulator >= frameInterval) {
    accumulator -= frameInterval;
    ++frameIndex;

    if (frameIndex >= static_cast<int>(animationFrames.size())) {
      if (loop)
        frameIndex = 0;
      else {
        frameIndex = static_cast<int>(animationFrames.size()) - 1;
        animationPlaying = false;
        stopTimer();
      }
    }

    juce::Component::SafePointer<MatrixDisplay> safeThis(this);
    if (onFrame)
      onFrame(frameIndex);
    if (safeThis == nullptr)
      return;
    if (!animationPlaying || animationFrames.empty())
      break;
  }

  if (frameIndex >= 0 && frameIndex < static_cast<int>(animationFrames.size()))
    currentFrame = animationFrames[static_cast<std::size_t>(frameIndex)];
  repaint();
}

Frame MatrixDisplay::ensureFrameSize(const Frame& frame) const {
  Frame result(static_cast<std::size_t>(rows),
               std::vector<float>(static_cast<std::size_t>(cols), 0.0f));
  static const std::vector<float> emptyRow;

  for (int r = 0; r < rows; ++r) {
    const auto& sourceRow =
        r < static_cast<int>(frame.size()) ? frame[static_cast<std::size_t>(r)] : emptyRow;

    for (int c = 0; c < cols; ++c) {
      const float value =
          c < static_cast<int>(sourceRow.size()) ? sourceRow[static_cast<std::size_t>(c)] : 0.0f;
      result[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] =
          std::isfinite(value) ? juce::jlimit(0.0f, 1.0f, value) : 0.0f;
    }
  }

  return result;
}

//==============================================================================
// MatrixAnimations

namespace MatrixAnimations {

std::vector<Frame> createLoader() {
  // From matrix.tsx loader
  std::vector<Frame> frames;
  const int size = 7;
  const int center = 3;
  const float radius = 2.5f;

  for (int frame = 0; frame < 12; ++frame) {
    Frame f = createEmptyFrame(size, size);

    for (int i = 0; i < 8; ++i) {
      const float angle = (static_cast<float>(frame) / 12.0f) * juce::MathConstants<float>::twoPi +
                          (static_cast<float>(i) / 8.0f) * juce::MathConstants<float>::twoPi;

      const int x = static_cast<int>(std::round(center + std::cos(angle) * radius));
      const int y = static_cast<int>(std::round(center + std::sin(angle) * radius));

      const float brightness = 1.0f - i / 10.0f;

      if (y >= 0 && y < size && x >= 0 && x < size) {
        f[y][x] = std::max(0.2f, brightness);
      }
    }

    frames.push_back(f);
  }

  return frames;
}

std::vector<Frame> createPulse() {
  // From matrix.tsx pulse
  std::vector<Frame> frames;
  const int size = 7;
  const int center = 3;

  for (int frame = 0; frame < 16; ++frame) {
    Frame f = createEmptyFrame(size, size);

    const float phase = (static_cast<float>(frame) / 16.0f) * juce::MathConstants<float>::twoPi;
    const float intensity = (std::sin(phase) + 1.0f) / 2.0f;

    // Center point
    f[center][center] = 1.0f;

    // Expanding ring
    const int radius = static_cast<int>((1.0f - intensity) * 3.0f) + 1;

    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        const float dist = std::sqrt(static_cast<float>(dx * dx + dy * dy));

        if (std::abs(dist - radius) < 0.7f) {
          const int py = center + dy;
          const int px = center + dx;

          if (py >= 0 && py < size && px >= 0 && px < size) {
            f[py][px] = intensity * 0.6f;
          }
        }
      }
    }

    frames.push_back(f);
  }

  return frames;
}

std::vector<Frame> createWave() {
  // From matrix.tsx wave
  std::vector<Frame> frames;
  const int rows = 7;
  const int cols = 7;

  for (int frame = 0; frame < 24; ++frame) {
    Frame f = createEmptyFrame(rows, cols);

    const float phase = (static_cast<float>(frame) / 24.0f) * juce::MathConstants<float>::twoPi;

    for (int col = 0; col < cols; ++col) {
      const float colPhase = (static_cast<float>(col) / cols) * juce::MathConstants<float>::twoPi;
      const float height = std::sin(phase + colPhase) * 2.5f + 3.5f;
      const int row = static_cast<int>(height);

      if (row >= 0 && row < rows) {
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

std::vector<Frame> createSnake() {
  // From matrix.tsx snake
  std::vector<Frame> frames;
  const int rows = 7;
  const int cols = 7;

  // Generate snake path
  std::vector<std::pair<int, int>> path;
  int x = 0, y = 0;
  int dx = 1, dy = 0;

  std::set<std::pair<int, int>> visited;

  while (static_cast<int>(path.size()) < rows * cols) {
    path.push_back({y, x});
    visited.insert({y, x});

    int nextX = x + dx;
    int nextY = y + dy;

    if (nextX >= 0 && nextX < cols && nextY >= 0 && nextY < rows &&
        visited.find({nextY, nextX}) == visited.end()) {
      x = nextX;
      y = nextY;
    } else {
      // Turn
      int newDx = -dy;
      int newDy = dx;
      dx = newDx;
      dy = newDy;

      nextX = x + dx;
      nextY = y + dy;

      if (nextX >= 0 && nextX < cols && nextY >= 0 && nextY < rows &&
          visited.find({nextY, nextX}) == visited.end()) {
        x = nextX;
        y = nextY;
      } else {
        break;
      }
    }
  }

  // Generate frames
  const int snakeLength = 5;

  for (size_t frame = 0; frame < path.size(); ++frame) {
    Frame f = createEmptyFrame(rows, cols);

    for (int i = 0; i < snakeLength; ++i) {
      const int idx = static_cast<int>(frame) - i;

      if (idx >= 0 && idx < static_cast<int>(path.size())) {
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

static void initializeSymbols() {
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
      {0, 0, 0, 1, 0}, {0, 0, 1, 0, 0}, {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1, 0}};

  s_chevronRight = {
      {0, 1, 0, 0, 0}, {0, 0, 1, 0, 0}, {0, 0, 0, 1, 0}, {0, 0, 1, 0, 0}, {0, 1, 0, 0, 0}};

  s_initialized = true;
}

const std::vector<Frame>& getDigits() {
  initializeSymbols();
  return s_digits;
}

const Frame& getChevronLeft() {
  initializeSymbols();
  return s_chevronLeft;
}

const Frame& getChevronRight() {
  initializeSymbols();
  return s_chevronRight;
}

} // namespace MatrixAnimations

} // namespace shmui
