#include "Components/AudioPlayerControls.h"
#include "Components/BarVisualizer.h"
#include "Components/LevelMeter.h"
#include "Components/MatrixDisplay.h"
#include "Components/MeterGroup.h"
#include "Components/ScrubBar.h"
#include "Components/TransportBar.h"
#include "Components/TransportContainer.h"
#include "Components/WaveformEditor.h"
#include "Controls/ClipButton.h"
#include "Controls/MuteButton.h"
#include "Controls/ToggleButton.h"
#include "Controls/TransportButton.h"
#include "Utils/ShmuiTheme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#if JUCE_MAC || JUCE_IOS
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace {
int failures = 0;

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::fprintf(stderr, "SHM015 failure: %s\n", message);
  }
}

void pumpMessageThread(int milliseconds) {
#if JUCE_MODAL_LOOPS_PERMITTED
  if (auto* manager = juce::MessageManager::getInstanceWithoutCreating())
    manager->runDispatchLoopUntil(juce::jmax(1, milliseconds));
#elif JUCE_MAC || JUCE_IOS
  CFRunLoopRunInMode(kCFRunLoopDefaultMode, static_cast<CFTimeInterval>(milliseconds) / 1000.0,
                     true);
#else
  juce::Thread::sleep(juce::jmax(1, milliseconds));
  juce::Timer::callPendingTimersSynchronously();
#endif
}

juce::MouseEvent makeMouseEvent(juce::Component& component, juce::Point<float> position,
                                juce::ModifierKeys modifiers = {}) {
  const auto source = juce::Desktop::getInstance().getMainMouseSource();
  const auto now = juce::Time::getCurrentTime();
  return juce::MouseEvent(source, position, modifiers, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component,
                          &component, now, position, now, 1, false);
}

juce::File makeWaveFile(const juce::String& name, int sampleCount) {
  const auto file =
      juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(name + ".wav");
  file.deleteFile();

  juce::WavAudioFormat format;
  auto output = std::unique_ptr<juce::FileOutputStream>(file.createOutputStream());
  if (output == nullptr)
    return {};

  auto* rawOutput = output.get();
  auto writer = std::unique_ptr<juce::AudioFormatWriter>(
      format.createWriterFor(rawOutput, 48000.0, 1, 16, {}, 0));
  if (writer == nullptr)
    return {};
  output.release();

  juce::AudioBuffer<float> buffer(1, sampleCount);
  buffer.clear();
  buffer.setSample(0, sampleCount / 2, 0.5f);
  require(writer->writeFromAudioSampleBuffer(buffer, 0, sampleCount),
          "fixture WAV writer accepts deterministic input");
  return file;
}

struct OrderedListener final : shmui::ThemeListener {
  OrderedListener(std::vector<int>& orderToUse, int idToUse) : order(orderToUse), id(idToUse) {}

  void defaultThemeChanged(const shmui::ShmuiTheme&) override {
    order.push_back(id);
  }

  std::vector<int>& order;
  int id;
};

struct ReentrantListener final : shmui::ThemeListener {
  ReentrantListener(std::vector<int>& orderToUse, shmui::ShmuiTheme pendingToUse)
      : order(orderToUse), pending(std::move(pendingToUse)) {}

  void defaultThemeChanged(const shmui::ShmuiTheme&) override {
    order.push_back(1);
    if (!triggered) {
      triggered = true;
      shmui::setDefaultTheme(pending);
    }
  }

  std::vector<int>& order;
  shmui::ShmuiTheme pending;
  bool triggered = false;
};

struct DestroyingScrubListener final : shmui::ScrubBar::Listener {
  explicit DestroyingScrubListener(std::unique_ptr<shmui::ScrubBar>* ownerToUse,
                                   bool& receivedToUse)
      : owner(ownerToUse), received(receivedToUse) {}

  void seekRequested(double) override {
    received = true;
    owner->reset();
  }

  std::unique_ptr<shmui::ScrubBar>* owner;
  bool& received;
};

struct DestroyingAudioListener final : shmui::AudioPlayerControls::Listener {
  explicit DestroyingAudioListener(std::unique_ptr<shmui::AudioPlayerControls>* ownerToUse,
                                   bool& receivedToUse)
      : owner(ownerToUse), received(receivedToUse) {}

  void playStateChanged(bool) override {
    received = true;
    owner->reset();
  }

  std::unique_ptr<shmui::AudioPlayerControls>* owner;
  bool& received;
};
struct DestroyingAudioSeekListener final : shmui::AudioPlayerControls::Listener {
  explicit DestroyingAudioSeekListener(std::unique_ptr<shmui::AudioPlayerControls>* ownerToUse,
                                       bool& receivedToUse)
      : owner(ownerToUse), received(receivedToUse) {}

  void seekRequested(double) override {
    received = true;
    owner->reset();
  }

  std::unique_ptr<shmui::AudioPlayerControls>* owner;
  bool& received;
};
struct TestClipButton final : shmui::ClipButton {
  using shmui::ClipButton::ClipButton;

  void animationTickForTest() {
    animationTick();
  }
};
} // namespace

int main() {
  juce::ScopedJuceInitialiser_GUI juceInitialiser;

  // Waveform generation is bounded, generation-invalidated, and safe when the
  // owner disappears while a worker is still finishing.
  const auto firstFile = makeWaveFile("shmui-shm015-first", 1024);
  const auto secondFile = makeWaveFile("shmui-shm015-second", 4096);
  require(firstFile.existsAsFile() && secondFile.existsAsFile(), "fixture WAV files exist");

  {
    shmui::WaveformEditor editor;
    editor.setAudioFile(firstFile);
    editor.setAudioFile(secondFile);
    for (int i = 0; i < 500 && !editor.hasData(); ++i)
      pumpMessageThread(10);
    require(editor.hasData(), "latest waveform generation publishes data");
    require(editor.getTrimOutSamples() == 4096,
            "stale waveform generation cannot overwrite the latest request");
  }

  {
    auto transient = std::make_unique<shmui::WaveformEditor>();
    transient->setAudioFile(firstFile);
    transient.reset();
    pumpMessageThread(40);
  }

  const auto invalidFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("shmui-shm015-invalid.wav");
  invalidFile.replaceWithText("not an audio file");
  {
    shmui::WaveformEditor editor;
    editor.setAudioFile(invalidFile);
    pumpMessageThread(40);
    require(!editor.hasData(), "invalid waveform input stays invalid");
  }

  // Matrix callback reentrancy clears the frame storage during dispatch.
  {
    shmui::MatrixDisplay matrix;
    matrix.setFrames({{{1.0f}}, {{0.25f}}}, 240.0f, true);
    int callbackCount = 0;
    matrix.onFrame = [&matrix, &callbackCount](int) {
      ++callbackCount;
      matrix.clear();
    };
    matrix.play();
    pumpMessageThread(40);
    require(callbackCount > 0, "matrix callback is dispatched");
    require(!matrix.isPlaying(), "matrix clear stops reentrant animation");
  }

  // Theme notifications preserve registration order and queue a reentrant
  // theme update instead of recursively invalidating the listener snapshot.
  {
    const auto firstTheme = shmui::ShmuiTheme::lab();
    auto secondTheme = shmui::ShmuiTheme::lab();
    secondTheme.accent = juce::Colour(0xff102030);
    std::vector<int> order;
    ReentrantListener first(order, secondTheme);
    OrderedListener second(order, 2);
    shmui::addDefaultThemeListener(&first);
    shmui::addDefaultThemeListener(&second);
    shmui::setDefaultTheme(firstTheme);
    require(order == std::vector<int>({1, 2, 1, 2}),
            "theme listener ordering survives reentrant updates");
    require(shmui::defaultTheme().accent == secondTheme.accent,
            "reentrant theme update becomes the final snapshot");
  }

  {
    auto updateTheme = shmui::ShmuiTheme::lab();
    updateTheme.accent = juce::Colour(0xff345678);
    updateTheme.fg = juce::Colour(0xffabcdef);
    updateTheme.fgMuted = juce::Colour(0xff789abc);

    shmui::AudioPlayerControls followsTheme;
    shmui::AudioPlayerControls customControls;
    auto customStyle = customControls.getStyle();
    customStyle.iconColor = juce::Colour(0xffedcba9);
    customControls.setStyle(customStyle);

    shmui::ScrubBar customScrub;
    auto scrubStyle = customScrub.getStyle();
    scrubStyle.thumbColor = juce::Colour(0xffcc8844);
    customScrub.setStyle(scrubStyle);

    shmui::TransportBar customTransport;
    auto transportStyle = customTransport.getStyle();
    transportStyle.textColor = juce::Colour(0xff44cc88);
    customTransport.setStyle(transportStyle);

    shmui::setDefaultTheme(updateTheme);
    require(followsTheme.getStyle().buttonColor == updateTheme.accent,
            "default-following control adopts theme accent");
    require(customControls.getStyle().iconColor == customStyle.iconColor,
            "custom audio control color wins over theme updates");
    require(customScrub.getStyle().thumbColor == scrubStyle.thumbColor,
            "custom scrub color wins over theme updates");
    require(customTransport.getStyle().textColor == transportStyle.textColor,
            "custom transport color wins over theme updates");
  }

  // Invalid geometry and arithmetic inputs remain finite and bounded.
  {
    shmui::AudioPlayerControls controls;
    auto style = controls.getStyle();
    style.buttonSize = std::numeric_limits<float>::quiet_NaN();
    style.padding = std::numeric_limits<float>::infinity();
    controls.setStyle(style);
    require(std::isfinite(controls.getStyle().buttonSize) &&
                std::isfinite(controls.getStyle().padding),
            "audio control geometry is sanitized");
    controls.setDuration(std::numeric_limits<double>::infinity());
    controls.setCurrentTime(std::numeric_limits<double>::infinity());
    require(std::isfinite(controls.getPlaybackRate()), "audio control state remains finite");

    shmui::ScrubBar scrub;
    auto scrubStyle = scrub.getStyle();
    scrubStyle.trackHeight = std::numeric_limits<float>::quiet_NaN();
    scrubStyle.thumbSize = std::numeric_limits<float>::infinity();
    scrub.setStyle(scrubStyle);
    scrub.setDuration(std::numeric_limits<double>::infinity());
    scrub.setCurrentTime(std::numeric_limits<double>::infinity());
    require(std::isfinite(scrub.getStyle().trackHeight) &&
                std::isfinite(scrub.getStyle().thumbSize) && std::isfinite(scrub.getCurrentTime()),
            "scrub geometry and time are sanitized");

    shmui::TransportBar transport;
    transport.setPositionSamples(std::numeric_limits<int64_t>::max(), 0);
    transport.setDurationSeconds(std::numeric_limits<double>::infinity());
    transport.setTempo(std::numeric_limits<double>::infinity());
    require(std::isfinite(transport.getPositionSeconds()) &&
                std::isfinite(transport.getDurationSeconds()) &&
                std::isfinite(transport.getTempo()),
            "transport arithmetic remains finite");

    shmui::LevelMeter meter;
    meter.setDBRange(std::numeric_limits<float>::quiet_NaN(),
                     std::numeric_limits<float>::infinity());
    meter.setLevel(0, std::numeric_limits<float>::quiet_NaN());
    meter.setLevelDB(0, std::numeric_limits<float>::infinity());

    shmui::MatrixDisplay matrix;
    matrix.setSize(-100, 10000);
    matrix.setFPS(std::numeric_limits<float>::quiet_NaN());
    require(matrix.getRowCount() == 1 && matrix.getColumnCount() == 128,
            "matrix dimensions are bounded");

    shmui::WaveformEditor editor;
    auto waveformStyle = editor.getStyle();
    waveformStyle.playheadWidth = std::numeric_limits<float>::quiet_NaN();
    editor.setStyle(waveformStyle);
    require(std::isfinite(editor.getStyle().playheadWidth), "waveform geometry is sanitized");
  }

  {
    auto editor = std::make_unique<shmui::WaveformEditor>();
    shmui::WaveformData data;
    data.sampleRate = 48000;
    data.numChannels = 1;
    data.totalSamples = 1000;
    data.minValues.assign(4, -0.5f);
    data.maxValues.assign(4, 0.5f);
    data.isValid = true;
    editor->setWaveformData(data);
    editor->setSize(100, 40);

    bool received = false;
    editor->onSeek = [&editor, &received](int64_t) {
      received = true;
      editor.reset();
    };
    auto event = makeMouseEvent(*editor, {50.0f, 10.0f}, juce::ModifierKeys::leftButtonModifier);
    editor->mouseDown(event);
    require(received && editor == nullptr, "waveform seek callback tolerates owner destruction");
  }

  {
    auto group = std::make_unique<shmui::MeterGroup>(1, true);
    group->enableHistory(4);
    bool received = false;
    group->onLevelEvent = [&group, &received](const shmui::LevelEvent&) {
      received = true;
      group.reset();
    };
    group->getGroupMeter(0).onLevelEvent(shmui::LevelEvent{});
    require(received && group == nullptr, "meter group callback tolerates owner destruction");
  }

  // Seek dispatch is a real mouse interaction, and the owner may disappear
  // from either listener boundary without a use-after-free.
  {
    auto scrub = std::make_unique<shmui::ScrubBar>();
    scrub->setSize(100, 20);
    scrub->setDuration(10.0);
    bool received = false;
    DestroyingScrubListener listener(&scrub, received);
    scrub->addListener(&listener);
    auto event = makeMouseEvent(*scrub, {50.0f, 10.0f}, juce::ModifierKeys::leftButtonModifier);
    scrub->mouseDown(event);
    require(received && scrub == nullptr, "scrub seek callback tolerates owner destruction");
  }

  {
    auto controls = std::make_unique<shmui::AudioPlayerControls>();
    bool received = false;
    DestroyingAudioListener listener(&controls, received);
    controls->addListener(&listener);
    controls->setPlaying(true);
    require(received && controls == nullptr, "audio listener callback tolerates owner destruction");
  }

  {
    auto controls = std::make_unique<shmui::AudioPlayerControls>();
    controls->setSize(300, 60);
    controls->setDuration(10.0);
    bool received = false;
    DestroyingAudioSeekListener listener(&controls, received);
    controls->addListener(&listener);
    auto event = makeMouseEvent(*controls, {150.0f, 30.0f}, juce::ModifierKeys::leftButtonModifier);
    controls->mouseDown(event);
    controls->mouseUp(event);
    require(received && controls == nullptr, "audio seek callback tolerates owner destruction");
  }

  // The asynchronous speed popup captures only a SafePointer to its owner.
  {
    auto controls = std::make_unique<shmui::AudioPlayerControls>();
    controls->setSize(200, 60);
    auto event = makeMouseEvent(*controls, {180.0f, 30.0f});
    controls->mouseDown(event);
    controls.reset();
    pumpMessageThread(40);
  }

  // ClipButton's declared callback surface compiles and Stopping resolves on
  // the next message-thread animation tick.
  {
    TestClipButton clip(0);
    clip.onClipClick = [](int) {};
    clip.setClipState(shmui::ClipButton::State::Loaded);
    clip.setClipState(shmui::ClipButton::State::Stopping);
    clip.animationTickForTest();
    require(clip.getClipState() == shmui::ClipButton::State::Loaded,
            "clip stopping state resolves to loaded on an animation tick");
  }

  // Button-family callbacks use owner tokens and retain the compatibility
  // dark-theme state without owning the default theme palette.
  {
    auto toggle = std::make_unique<shmui::ToggleButton>(shmui::IconType::Mute);
    toggle->setDarkTheme(true);
    require(toggle->isDarkTheme(), "dark-theme compatibility state is retained");
    toggle->onToggle = [&toggle](bool) { toggle.reset(); };
    toggle->juce::Component::setSize(40, 40);
    auto event = makeMouseEvent(*toggle, {20.0f, 20.0f}, juce::ModifierKeys::leftButtonModifier);
    toggle->mouseDown(event);
    toggle->mouseUp(event);
    require(toggle == nullptr, "toggle callback tolerates owner destruction");

    auto mute = std::make_unique<shmui::MuteButton>(shmui::MuteButton::Type::Mute);
    mute->onToggle = [&mute](bool) { mute.reset(); };
    mute->juce::Component::setSize(40, 40);
    auto muteEvent = makeMouseEvent(*mute, {20.0f, 20.0f}, juce::ModifierKeys::leftButtonModifier);
    mute->mouseDown(muteEvent);
    mute->mouseUp(muteEvent);
    require(mute == nullptr, "mute callback tolerates owner destruction");

    auto transportButton =
        std::make_unique<shmui::TransportButton>(shmui::TransportButton::Type::PlayPause);
    transportButton->onClick = [&transportButton] { transportButton.reset(); };
    transportButton->juce::Component::setSize(40, 40);
    auto transportEvent =
        makeMouseEvent(*transportButton, {20.0f, 20.0f}, juce::ModifierKeys::leftButtonModifier);
    transportButton->mouseDown(transportEvent);
    transportButton->mouseUp(transportEvent);
    require(transportButton == nullptr, "transport button callback tolerates owner destruction");
  }

  shmui::setDefaultTheme(shmui::ShmuiTheme::lab());
  invalidFile.deleteFile();
  firstFile.deleteFile();
  secondFile.deleteFile();
  pumpMessageThread(20);
  return failures == 0 ? 0 : 1;
}
