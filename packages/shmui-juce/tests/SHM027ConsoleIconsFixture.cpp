/*
  ==============================================================================

    SHM027ConsoleIconsFixture.cpp
    Native boundary fixture for the shared console icon set.

    The fixture proves three things about the shared ShmUI icon layer:

      1. Every console role resolves to real geometry, and the geometry still
         reproduces the upstream artwork it was sourced from. The reference
         numbers in SHM027ConsoleIconsReference.h are measured from the
         upstream SVG files by scripts/gen-console-icon-geometry.py, so the
         native JUCE render is compared against the artwork itself.
      2. The render contract holds: the open-geometry console marks are
         stroked (not filled), the filled marks stay filled, and the four
         morphic-control-grid lamps share one optical height.
      3. The shared controls that consume those roles - the transport button,
         the transport bar toggle and the clip button badges - render the
         selected marks rather than a local fallback.

    Gallery mode renders the qualified evidence used by the SHM027 handoff:

      shmui_shm027_console_icons_fixture --gallery <output directory>

  ==============================================================================
*/

#include "Icons/Icons.h"

#include "Controls/ClipButton.h"
#include "Controls/ToggleButton.h"
#include "Controls/TransportButton.h"

#include "SHM027ConsoleIconsReference.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void require(bool condition, const juce::String& message) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", message.toRawUTF8());
  }
}

//==============================================================================
struct Role {
  shmui::IconType type;
  const char* name;
};

const Role consoleRoles[] = {
    {shmui::IconType::Loop, "Loop"},
    {shmui::IconType::StopPeers, "Stop Peers"},
    {shmui::IconType::FadeIn, "Fade In"},
    {shmui::IconType::FadeOut, "Fade Out"},
    {shmui::IconType::Shift, "Shift"},
    {shmui::IconType::Play, "Play"},
    {shmui::IconType::Pause, "Pause"},
    {shmui::IconType::Stop, "Stop"},
    {shmui::IconType::Record, "Record"},
    {shmui::IconType::Rewind, "Rewind"},
    {shmui::IconType::FastForward, "Fast Forward"},
    {shmui::IconType::SkipPrev, "Skip Previous"},
    {shmui::IconType::SkipNext, "Skip Next"},
};

const Role lampRoles[] = {
    {shmui::IconType::Loop, "Loop"},
    {shmui::IconType::StopPeers, "Stop Peers"},
    {shmui::IconType::FadeIn, "Fade In"},
    {shmui::IconType::FadeOut, "Fade Out"},
};

//==============================================================================
struct InkStats {
  double coverage = 0.0;
  double centroidX = 0.0;
  double centroidY = 0.0;
  int minX = 0;
  int minY = 0;
  int maxX = 0;
  int maxY = 0;
  bool empty = true;

  int width() const {
    return empty ? 0 : maxX - minX + 1;
  }
  int height() const {
    return empty ? 0 : maxY - minY + 1;
  }
};

/** Render one icon through the public shared API at a logical size and raster scale. */
juce::Image renderIcon(shmui::IconType type, int logicalSize, int rasterScale) {
  const auto pixels = logicalSize * rasterScale;
  juce::Image image(juce::Image::ARGB, pixels, pixels, true);
  juce::Graphics g(image);
  g.addTransform(juce::AffineTransform::scale((float)rasterScale, (float)rasterScale));

  const juce::Rectangle<float> box((float)logicalSize, (float)logicalSize);
  shmui::Icons::drawIcon(g, type, box, juce::Colours::white);
  return image;
}

/** Render a component through its real paint path. */
juce::Image renderComponent(juce::Component& component, int size) {
  juce::Image image(juce::Image::ARGB, size, size, true);
  juce::Graphics g(image);
  component.paintEntireComponent(g, true);
  return image;
}

InkStats measure(const juce::Image& image) {
  InkStats stats;
  double total = 0.0;
  double sumX = 0.0;
  double sumY = 0.0;

  const auto width = image.getWidth();
  const auto height = image.getHeight();

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const double alpha = image.getPixelAt(x, y).getAlpha() / 255.0;

      if (alpha <= 0.0)
        continue;

      if (stats.empty) {
        stats.minX = stats.maxX = x;
        stats.minY = stats.maxY = y;
        stats.empty = false;
      } else {
        stats.minX = std::min(stats.minX, x);
        stats.maxX = std::max(stats.maxX, x);
        stats.minY = std::min(stats.minY, y);
        stats.maxY = std::max(stats.maxY, y);
      }

      total += alpha;
      sumX += alpha * (x + 0.5);
      sumY += alpha * (y + 0.5);
    }
  }

  if (total > 0.0) {
    stats.coverage = total / ((double)width * (double)height);
    stats.centroidX = sumX / total / (double)width;
    stats.centroidY = sumY / total / (double)height;
  }

  return stats;
}

/** The inked extent of one row of an image, as [first x, last x]. */
void rowExtent(const juce::Image& image, int y, int& firstX, int& lastX) {
  firstX = -1;
  lastX = -1;

  for (int x = 0; x < image.getWidth(); ++x) {
    if (image.getPixelAt(x, y).getAlpha() > 8) {
      if (firstX < 0)
        firstX = x;
      lastX = x;
    }
  }
}

/** The inked extent of one column of an image, as [first y, last y]. */
void columnExtent(const juce::Image& image, int x, int& firstY, int& lastY) {
  firstY = -1;
  lastY = -1;

  for (int y = 0; y < image.getHeight(); ++y) {
    if (image.getPixelAt(x, y).getAlpha() > 8) {
      if (firstY < 0)
        firstY = y;
      lastY = y;
    }
  }
}

//==============================================================================
void requireEveryRoleHasGeometry() {
  for (const auto& role : consoleRoles) {
    const auto path = shmui::Icons::getIcon(role.type, 24.0f);
    require(!path.isEmpty(), juce::String(role.name) + " resolves to geometry");

    const auto bounds = path.getBounds();
    require(bounds.getWidth() > 0.0f && bounds.getHeight() > 0.0f,
            juce::String(role.name) + " has a non-degenerate bounding box");
  }
}

//==============================================================================
void requireRenderingMatchesUpstreamArtwork() {
  for (const auto& reference : shmui::SHM027::glyphReferences) {
    const auto image = renderIcon(reference.type, (int)reference.size, 1);
    const auto stats = measure(image);

    require(!stats.empty, juce::String(shmui::Icons::getIconName(reference.type)) +
                              " renders ink at the reference size");

    require(std::abs(stats.coverage - reference.ink.coverage) <= 0.02,
            juce::String(shmui::Icons::getIconName(reference.type)) +
                " covers the same ink area as the upstream artwork (measured " +
                juce::String(stats.coverage, 4) + ", upstream " +
                juce::String(reference.ink.coverage, 4) + ")");

    require(std::abs(stats.centroidX - reference.ink.centroidX) <= 0.03,
            juce::String(shmui::Icons::getIconName(reference.type)) +
                " has the same horizontal ink centroid as the upstream artwork (measured " +
                juce::String(stats.centroidX, 4) + ", upstream " +
                juce::String(reference.ink.centroidX, 4) + ")");

    require(std::abs(stats.centroidY - reference.ink.centroidY) <= 0.03,
            juce::String(shmui::Icons::getIconName(reference.type)) +
                " has the same vertical ink centroid as the upstream artwork (measured " +
                juce::String(stats.centroidY, 4) + ", upstream " +
                juce::String(reference.ink.centroidY, 4) + ")");
  }
}

//==============================================================================
void requireRenderContract() {
  require(shmui::Icons::getIconRenderSpec(shmui::IconType::Loop, 16.0f).mode ==
              shmui::IconRenderMode::Stroked,
          "the loop mark is stroked");

  require(shmui::Icons::getIconRenderSpec(shmui::IconType::StopPeers, 16.0f).mode ==
              shmui::IconRenderMode::Stroked,
          "the stop-peers badge is stroked");

  require(std::abs(shmui::Icons::getIconRenderSpec(shmui::IconType::Loop, 16.0f).strokeWidth -
                   1.50f) < 0.001f,
          "the loop stroke follows the console policy at 16 px");

  for (int i = 0; i < (int)shmui::IconType::NumIcons; ++i) {
    const auto type = (shmui::IconType)i;

    switch (type) {
    case shmui::IconType::Loop:
    case shmui::IconType::StopPeers:
      break;

    default:
      require(shmui::Icons::getIconRenderSpec(type, 16.0f).mode == shmui::IconRenderMode::Filled,
              juce::String(shmui::Icons::getIconName(type)) + " stays a filled icon");
      break;
    }
  }

  require(std::abs(shmui::Icons::getConsoleStrokeWidth(11.0f) - 1.10f) < 0.001f,
          "the console stroke policy is 1.10 px at 11 px");

  require(std::abs(shmui::Icons::getConsoleStrokeWidth(14.0f) - 1.25f) < 0.001f,
          "the console stroke policy is 1.25 px at 14 px");

  require(std::abs(shmui::Icons::getConsoleStrokeWidth(16.0f) - 1.50f) < 0.001f,
          "the console stroke policy is 1.50 px at 16 px");

  require(shmui::Icons::getConsoleStrokeWidth(10.0f) < shmui::Icons::getConsoleStrokeWidth(11.0f) &&
              shmui::Icons::getConsoleStrokeWidth(16.0f) <
                  shmui::Icons::getConsoleStrokeWidth(24.0f),
          "the console stroke policy grows with the surface size");
}

//==============================================================================
void requireOpenGeometryIsStrokedNotFilled() {
  // The loop is open geometry: if the shared layer ever filled it, the mark
  // would collapse into a solid blob. The interior has to stay open.
  const auto image = renderIcon(shmui::IconType::Loop, 32, 1);
  const auto stats = measure(image);

  require(!stats.empty, "the loop mark renders ink");

  if (!stats.empty) {
    const auto centreX = (stats.minX + stats.maxX) / 2;
    const auto centreY = (stats.minY + stats.maxY) / 2;
    require(image.getPixelAt(centreX, centreY).getAlpha() < 48,
            "the loop interior stays open (the mark is stroked, not filled)");
  }

  // The stop-peers badge is an outline hexagon, so its middle is open too.
  const auto badge = renderIcon(shmui::IconType::StopPeers, 32, 1);
  const auto badgeStats = measure(badge);

  if (!badgeStats.empty) {
    const auto centreX = (badgeStats.minX + badgeStats.maxX) / 2;
    const auto centreY = (badgeStats.minY + badgeStats.maxY) / 2;
    require(badge.getPixelAt(centreX, centreY).getAlpha() < 48,
            "the stop-peers badge interior stays open (the mark is stroked, not filled)");
  }
}

//==============================================================================
void requireStopPeersIsAFlatToppedHexagon() {
  // The traffic-sign hexagon: flat top and bottom, points left and right, and
  // a width/height ratio of 2/sqrt(3).
  const auto path = shmui::Icons::getIcon(shmui::IconType::StopPeers, 24.0f);
  const auto bounds = path.getBounds();
  const auto ratio = bounds.getWidth() / bounds.getHeight();

  require(std::abs(ratio - 2.0 / std::sqrt(3.0)) < 0.005f,
          "the stop-peers badge keeps the 2:sqrt(3) hexagon proportion");

  const auto image = renderIcon(shmui::IconType::StopPeers, 64, 1);
  const auto stats = measure(image);
  require(!stats.empty, "the stop-peers badge renders ink");

  if (!stats.empty) {
    int firstX = 0;
    int lastX = 0;
    int firstY = 0;
    int lastY = 0;

    rowExtent(image, stats.minY, firstX, lastX);
    const auto topWidth = (double)(lastX - firstX + 1);
    const auto totalWidth = (double)stats.width();
    require(topWidth / totalWidth > 0.35 && topWidth / totalWidth < 0.65,
            "the stop-peers badge has a flat top edge");

    rowExtent(image, stats.maxY, firstX, lastX);
    require((double)(lastX - firstX + 1) / totalWidth > 0.35,
            "the stop-peers badge has a flat bottom edge");

    columnExtent(image, stats.minX, firstY, lastY);
    require((double)(lastY - firstY + 1) / (double)stats.height() < 0.35,
            "the stop-peers badge points left rather than having a vertical edge");

    columnExtent(image, stats.maxX, firstY, lastY);
    require((double)(lastY - firstY + 1) / (double)stats.height() < 0.35,
            "the stop-peers badge points right rather than having a vertical edge");
  }
}

//==============================================================================
void requireFadeWedgesKeepTheirForm() {
  // Fade In rises to the right: its top edge is on the right-hand side and its
  // bottom edge is flat.
  const auto fadeIn = renderIcon(shmui::IconType::FadeIn, 48, 1);
  const auto fadeInStats = measure(fadeIn);
  require(!fadeInStats.empty, "the fade-in wedge renders ink");

  int firstX = 0;
  int lastX = 0;

  if (!fadeInStats.empty) {
    rowExtent(fadeIn, fadeInStats.minY, firstX, lastX);
    require((double)(firstX + lastX) / 2.0 > (double)fadeInStats.width() * 0.6,
            "the fade-in wedge rises to the right");

    rowExtent(fadeIn, fadeInStats.maxY, firstX, lastX);
    require((double)(lastX - firstX + 1) / (double)fadeInStats.width() > 0.85,
            "the fade-in wedge has a flat bottom edge");
  }

  // Fade Out falls away to the right. It is the horizontal mirror of the
  // fade-in, so its bottom edge is flat as well and both bottom corners share
  // one y-value.
  const auto fadeOut = renderIcon(shmui::IconType::FadeOut, 48, 1);
  const auto fadeOutStats = measure(fadeOut);
  require(!fadeOutStats.empty, "the fade-out wedge renders ink");

  if (!fadeOutStats.empty) {
    rowExtent(fadeOut, fadeOutStats.minY, firstX, lastX);
    require((double)(firstX + lastX) / 2.0 < (double)fadeOutStats.width() * 0.4,
            "the fade-out wedge falls away to the right");

    rowExtent(fadeOut, fadeOutStats.maxY, firstX, lastX);
    require((double)(lastX - firstX + 1) / (double)fadeOutStats.width() > 0.85,
            "the fade-out wedge has a flat bottom edge");

    require(fadeOutStats.width() == fadeInStats.width() &&
                fadeOutStats.height() == fadeInStats.height(),
            "the fade-out wedge is the horizontal mirror of the fade-in wedge");
  }
}

//==============================================================================
void requireLampMarksShareOneOpticalHeight() {
  for (const auto size : {11, 14, 16}) {
    double shortest = 1.0e9;
    double tallest = 0.0;

    for (const auto& role : lampRoles) {
      const auto image = renderIcon(role.type, size, 1);
      const auto stats = measure(image);
      require(!stats.empty,
              juce::String(role.name) + " renders ink at " + juce::String(size) + " px");

      const auto paintedHeight = (double)stats.height();
      shortest = std::min(shortest, paintedHeight);
      tallest = std::max(tallest, paintedHeight);
    }

    require(tallest / shortest <= 1.16,
            juce::String("the four lamps share one optical height at ") + juce::String(size) +
                " px");
  }
}

//==============================================================================
void requireMarksStayInsideTheirSurface() {
  for (const auto& role : consoleRoles) {
    for (const auto size : {11, 14, 16}) {
      for (const auto rasterScale : {1, 2}) {
        const auto image = renderIcon(role.type, size, rasterScale);
        const auto stats = measure(image);

        require(!stats.empty,
                juce::String(role.name) + " renders ink at " + juce::String(size) + " px");

        if (stats.empty)
          continue;

        require(stats.minX >= 0 && stats.minY >= 0 && stats.maxX < image.getWidth() &&
                    stats.maxY < image.getHeight(),
                juce::String(role.name) + " stays inside its surface at " + juce::String(size) +
                    " px");
      }
    }
  }
}

//==============================================================================
void requireSharedControlsRenderTheSelectedMarks() {
  // The clip button registers the shared lamp roles.
  shmui::ClipButton clipButton(0);
  require(clipButton.hasIndicator("loop"), "the clip button registers the shared loop badge");
  require(clipButton.hasIndicator("fadeIn"), "the clip button registers the shared fade-in badge");
  require(clipButton.hasIndicator("fadeOut"),
          "the clip button registers the shared fade-out badge");

  // The clip button's loop badge is the shared stroked loop: the badge interior
  // stays open, which a filled fallback could not do.
  clipButton.setBounds(0, 0, 48, 48);
  clipButton.setClipState(shmui::ClipButton::State::Loaded);

  clipButton.setIndicatorVisible("loop", true);
  const auto withBadge = renderComponent(clipButton, 48);
  clipButton.setIndicatorVisible("loop", false);
  const auto withoutBadge = renderComponent(clipButton, 48);

  juce::Image difference(juce::Image::ARGB, 48, 48, true);

  for (int y = 0; y < 48; ++y) {
    for (int x = 0; x < 48; ++x) {
      const auto delta = (int)withBadge.getPixelAt(x, y).getAlpha() -
                         (int)withoutBadge.getPixelAt(x, y).getAlpha();

      if (delta > 0)
        difference.setPixelAt(x, y, juce::Colours::white.withAlpha((juce::uint8)255));
    }
  }

  const auto badgeStats = measure(difference);
  require(!badgeStats.empty, "the clip button draws its loop badge");

  if (!badgeStats.empty) {
    const auto centreX = (badgeStats.minX + badgeStats.maxX) / 2;
    const auto centreY = (badgeStats.minY + badgeStats.maxY) / 2;
    require(difference.getPixelAt(centreX, centreY).getAlpha() < 48,
            "the clip button loop badge is the shared stroked loop");
  }

  // The transport bar toggle draws the same shared geometry.
  shmui::ToggleButton toggle(shmui::IconType::Loop);
  toggle.setBounds(0, 0, 48, 48);
  const auto toggleImage = renderComponent(toggle, 48);
  const auto toggleStats = measure(toggleImage);

  require(!toggleStats.empty, "the transport bar toggle draws its loop mark");

  // The transport button draws the shared stroked loop, so the mark's interior
  // stays open instead of collapsing into a filled blob.
  shmui::TransportButton loopButton(shmui::TransportButton::Type::Loop);
  loopButton.setBounds(0, 0, 48, 48);
  const auto loopImage = renderComponent(loopButton, 48);
  const auto loopStats = measure(loopImage);

  require(!loopStats.empty, "the transport loop button draws its mark");

  if (!loopStats.empty) {
    const auto centreX = (loopStats.minX + loopStats.maxX) / 2;
    const auto centreY = (loopStats.minY + loopStats.maxY) / 2;
    require(loopImage.getPixelAt(centreX, centreY).getAlpha() < 48,
            "the transport loop button renders the shared stroked loop");
  }
}

//==============================================================================
juce::Image renderRoleSheet(const std::vector<shmui::IconType>& roles, int logicalSize,
                            int rasterScale) {
  const int gap = 8;
  const int cell = logicalSize + gap;
  const int width = cell * (int)roles.size();
  const int height = logicalSize + gap * 2;

  juce::Image image(juce::Image::ARGB, width * rasterScale, height * rasterScale, true);
  juce::Graphics g(image);
  g.fillAll(juce::Colour(0xff1c1c1c));
  g.addTransform(juce::AffineTransform::scale((float)rasterScale, (float)rasterScale));

  for (size_t i = 0; i < roles.size(); ++i) {
    const auto box = juce::Rectangle<float>((float)(i * cell + gap / 2), (float)gap,
                                            (float)logicalSize, (float)logicalSize);
    shmui::Icons::drawIcon(g, roles[i], box, juce::Colours::white);
  }

  return image;
}

juce::Image renderControlSheet(int rasterScale) {
  const int cell = 56;
  const int width = cell * 13;
  const int height = cell + 16;

  juce::Image image(juce::Image::ARGB, width * rasterScale, height * rasterScale, true);
  juce::Graphics g(image);
  g.fillAll(juce::Colour(0xff1c1c1c));
  g.addTransform(juce::AffineTransform::scale((float)rasterScale, (float)rasterScale));

  const shmui::TransportButton::Type types[] = {
      shmui::TransportButton::Type::Play,     shmui::TransportButton::Type::Pause,
      shmui::TransportButton::Type::Stop,     shmui::TransportButton::Type::Record,
      shmui::TransportButton::Type::Rewind,   shmui::TransportButton::Type::FastForward,
      shmui::TransportButton::Type::SkipPrev, shmui::TransportButton::Type::SkipNext,
      shmui::TransportButton::Type::Loop,
  };

  for (size_t i = 0; i < juce::numElementsInArray(types); ++i) {
    shmui::TransportButton button(types[i]);
    button.setBounds((int)(i * cell), 8, cell - 8, cell - 16);

    juce::Graphics buttonGraphics(image);
    buttonGraphics.addTransform(juce::AffineTransform::translation((float)(i * cell), 0.0f));
    button.paintEntireComponent(buttonGraphics, true);
  }

  // The transport bar's loop toggle.
  shmui::ToggleButton toggle(shmui::IconType::Loop);
  toggle.setBounds(0, 0, cell - 8, cell - 16);

  {
    juce::Graphics toggleGraphics(image);
    toggleGraphics.addTransform(juce::AffineTransform::translation((float)(9 * cell), 0.0f));
    toggle.paintEntireComponent(toggleGraphics, true);
  }

  // One clip button per lamp badge, so each shared mark is visible on its own.
  const char* badges[] = {"loop", "fadeIn", "fadeOut"};

  for (int i = 0; i < 3; ++i) {
    shmui::ClipButton clip(i);
    clip.setBounds(0, 0, cell - 8, cell - 16);
    clip.setClipState(shmui::ClipButton::State::Loaded);
    clip.setIndicatorVisible(badges[i], true);

    juce::Graphics clipGraphics(image);
    clipGraphics.addTransform(juce::AffineTransform::translation((float)((10 + i) * cell), 0.0f));
    clip.paintEntireComponent(clipGraphics, true);
  }

  return image;
}

bool writePng(const juce::Image& image, const juce::File& file) {
  juce::PNGImageFormat png;
  juce::FileOutputStream stream(file);

  if (!stream.openedOk())
    return false;

  return png.writeImageToStream(image, stream);
}

int runGallery(const juce::File& directory) {
  directory.createDirectory();

  int written = 0;

  const std::vector<shmui::IconType> allRoles = {
      shmui::IconType::Loop,     shmui::IconType::StopPeers,   shmui::IconType::FadeIn,
      shmui::IconType::FadeOut,  shmui::IconType::Shift,       shmui::IconType::Play,
      shmui::IconType::Pause,    shmui::IconType::Stop,        shmui::IconType::Record,
      shmui::IconType::Rewind,   shmui::IconType::FastForward, shmui::IconType::SkipPrev,
      shmui::IconType::SkipNext,
  };

  const std::vector<shmui::IconType> lamps = {
      shmui::IconType::Loop,
      shmui::IconType::StopPeers,
      shmui::IconType::FadeIn,
      shmui::IconType::FadeOut,
  };

  const std::vector<shmui::IconType> transport = {
      shmui::IconType::Play,     shmui::IconType::Pause,    shmui::IconType::Stop,
      shmui::IconType::Record,   shmui::IconType::Rewind,   shmui::IconType::FastForward,
      shmui::IconType::SkipPrev, shmui::IconType::SkipNext, shmui::IconType::Shift,
  };

  for (const auto size : {11, 14, 16}) {
    for (const auto rasterScale : {1, 2}) {
      const auto suffix = juce::String(size) + "px-" + juce::String(rasterScale) + "x";

      for (const auto& sheet :
           {std::pair<const char*, std::vector<shmui::IconType>>{"console", allRoles},
            std::pair<const char*, std::vector<shmui::IconType>>{"lamps", lamps},
            std::pair<const char*, std::vector<shmui::IconType>>{"transport", transport}}) {
        const auto image = renderRoleSheet(sheet.second, size, rasterScale);
        const auto file = directory.getChildFile(juce::String(sheet.first) + "-" + suffix + ".png");

        if (writePng(image, file))
          ++written;
        else
          std::fprintf(stderr, "FAIL: could not write %s\n", file.getFullPathName().toRawUTF8());
      }
    }
  }

  for (const auto rasterScale : {1, 2}) {
    const auto image = renderControlSheet(rasterScale);
    const auto file =
        directory.getChildFile(juce::String("controls-") + juce::String(rasterScale) + "x.png");

    if (writePng(image, file))
      ++written;
    else
      std::fprintf(stderr, "FAIL: could not write %s\n", file.getFullPathName().toRawUTF8());
  }

  std::printf("wrote %d gallery images to %s\n", written, directory.getFullPathName().toRawUTF8());
  return 0;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
  juce::ScopedJuceInitialiser_GUI juceRuntime;

  juce::String galleryDirectory;

  for (int i = 1; i < argc; ++i) {
    if (juce::String(argv[i]) == "--gallery" && i + 1 < argc)
      galleryDirectory = juce::String(argv[i + 1]);
  }

  if (galleryDirectory.isNotEmpty()) {
    const auto target =
        juce::File::isAbsolutePath(galleryDirectory)
            ? juce::File(galleryDirectory)
            : juce::File::getCurrentWorkingDirectory().getChildFile(galleryDirectory);
    return runGallery(target);
  }

  requireEveryRoleHasGeometry();
  requireRenderingMatchesUpstreamArtwork();
  requireRenderContract();
  requireOpenGeometryIsStrokedNotFilled();
  requireStopPeersIsAFlatToppedHexagon();
  requireFadeWedgesKeepTheirForm();
  requireLampMarksShareOneOpticalHeight();
  requireMarksStayInsideTheirSurface();
  requireSharedControlsRenderTheSelectedMarks();

  if (failures == 0)
    std::printf("SHM027 console icon fixture: all checks passed\n");

  return failures == 0 ? 0 : 1;
}
