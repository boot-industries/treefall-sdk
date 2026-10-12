// SPDX-License-Identifier: MIT

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "otio/reconform_plan.h"
#include "render/render_tracks.h"
#include <orpheus/session_io.h>

#include <exception>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

namespace fs = std::filesystem;
using orpheus::core::SessionGraph;
namespace session_json = orpheus::core::session_json;
namespace reconform = orpheus::core::otio;

class OrpheusSessionController {
public:
  struct Snapshot {
    juce::String sessionName;
    juce::String sourcePath;
    std::size_t trackCount{};
    std::size_t clipCount{};
    double tempoBpm{};
    double rangeStart{};
    double rangeEnd{};
    std::uint32_t renderSampleRate{};
    std::uint16_t renderBitDepth{};
    bool renderDither{};
    bool clipgridCommitted{};
    juce::String lastRenderDirectory;

    bool operator==(const Snapshot&) const = default;
  };

  bool openSession(const juce::File& file, juce::String& error) {
    SessionGraph parsed;
    try {
      parsed = session_json::LoadSessionFromFile(file.getFullPathName().toStdString());
    } catch (const std::exception& ex) {
      error = juce::String("Session load failed: ") + ex.what();
      return false;
    }

    std::size_t newClipCount = 0;
    for (const auto& track : parsed.tracks())
      newClipCount += track->clips().size();

    graph_ = std::move(parsed);
    clipCount_ = newClipCount;
    sourcePath_ = file.getFullPathName();
    hasSession_ = true;
    clipgridCommitted_ = false;
    lastRenderDirectory_.clear();
    return true;
  }

  bool triggerClipgrid(juce::String& error) {
    if (!hasSession_) {
      error = "Load a session first";
      return false;
    }
    if (clipgridCommitted_)
      return true;

    try {
      graph_.commit_clip_grid();
    } catch (const std::exception& ex) {
      error = juce::String("ClipGrid commit failed: ") + ex.what();
      return false;
    }

    clipgridCommitted_ = true;
    return true;
  }

  bool renderToDirectory(const juce::File& directory, juce::String& error) {
    if (!hasSession_) {
      error = "Load a session first";
      return false;
    }
    if (!clipgridCommitted_) {
      error = "Trigger the ClipGrid scene before rendering";
      return false;
    }

    try {
      orpheus::core::render::render_session_tracks(
          graph_, fs::path(directory.getFullPathName().toStdString()));
    } catch (const std::exception& ex) {
      error = juce::String("Render failed: ") + ex.what();
      return false;
    }

    lastRenderDirectory_ = directory.getFullPathName();
    return true;
  }

  [[nodiscard]] bool hasSession() const noexcept {
    return hasSession_;
  }
  [[nodiscard]] bool clipgridCommitted() const noexcept {
    return clipgridCommitted_;
  }

  Snapshot snapshot() const {
    Snapshot snap{};
    snap.sessionName = graph_.name();
    snap.sourcePath = sourcePath_;
    snap.trackCount = graph_.tracks().size();
    snap.clipCount = clipCount_;
    snap.tempoBpm = graph_.tempo();
    snap.rangeStart = graph_.session_start_beats();
    snap.rangeEnd = graph_.session_end_beats();
    snap.renderSampleRate = graph_.render_sample_rate();
    snap.renderBitDepth = graph_.render_bit_depth();
    snap.renderDither = graph_.render_dither();
    snap.clipgridCommitted = clipgridCommitted_;
    snap.lastRenderDirectory = lastRenderDirectory_;
    return snap;
  }

private:
  SessionGraph graph_{};
  std::size_t clipCount_{};
  bool hasSession_{};
  juce::String sourcePath_;
  bool clipgridCommitted_{};
  juce::String lastRenderDirectory_;
};

class MainComponent : public juce::Component {
public:
  explicit MainComponent(bool initialiseAudioDevice = true) {
    addAndMakeVisible(header_);
    header_.setText("Orpheus Demo Host", juce::dontSendNotification);
    header_.setFont(juce::FontOptions{24.0f, juce::Font::bold});

    addAndMakeVisible(disclaimer_);
    disclaimer_.setJustificationType(juce::Justification::centredLeft);
    disclaimer_.setColour(juce::Label::textColourId, juce::Colours::orange);
    disclaimer_.setText(
        juce::String::fromUTF8("Evaluation build \xe2\x80\x93 renders synthetic stems only."),
        juce::dontSendNotification);

    addAndMakeVisible(statusBox_);
    statusBox_.setReadOnly(true);
    statusBox_.setMultiLine(true);
    statusBox_.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    statusBox_.setColour(juce::TextEditor::textColourId, juce::Colours::lightgreen);
    statusBox_.setFont(juce::FontOptions{14.0f});
    statusBox_.setText(initialInstructions(), juce::dontSendNotification);

    if (initialiseAudioDevice)
      audioDeviceManager_.initialise(0, 2, nullptr, true);
  }

  void updateSnapshot(const OrpheusSessionController::Snapshot& snapshot) {
    std::ostringstream stream;
    stream << "Session: " << snapshot.sessionName << '\n';
    stream << "Source: " << snapshot.sourcePath << '\n';
    stream << "Tempo: " << std::fixed << std::setprecision(2) << snapshot.tempoBpm << " BPM\n";
    stream << "Tracks: " << snapshot.trackCount << "\n";
    stream << "Clips: " << snapshot.clipCount << "\n";
    stream << "Range: " << snapshot.rangeStart << " → " << snapshot.rangeEnd << " beats\n";
    stream << "ClipGrid committed: " << (snapshot.clipgridCommitted ? "yes" : "no") << "\n";
    if (snapshot.lastRenderDirectory.isNotEmpty())
      stream << "Last render: " << snapshot.lastRenderDirectory << '\n';
    sessionSummary_ = stream.str();
    refreshStatusBox();
  }

  juce::String statusText() const {
    return statusBox_.getText();
  }

  void updateReconformPlan(const juce::String& planPath, const reconform::ReconformPlan& plan) {
    auto formatSeconds = [](double seconds) {
      std::ostringstream out;
      out.setf(std::ios::fixed);
      out << std::setprecision(2) << seconds;
      return out.str();
    };

    auto describeRange = [&](const reconform::ReconformTimeRange& range) {
      std::ostringstream out;
      out << "@" << formatSeconds(range.start_seconds) << "s for "
          << formatSeconds(range.duration_seconds) << "s";
      return out.str();
    };

    std::ostringstream stream;
    stream << "Reconform plan: " << plan.timeline_name << '\n';
    stream << "Plan file: " << planPath << '\n';
    stream << "Operations: " << plan.operations.size() << '\n';

    for (std::size_t index = 0; index < plan.operations.size(); ++index) {
      const auto& operation = plan.operations[index];
      stream << "  [" << (index + 1) << "] ";
      std::visit(
          [&](const auto& op) {
            using T = std::decay_t<decltype(op)>;
            if constexpr (std::is_same_v<T, reconform::ReconformInsert>) {
              stream << "Insert " << describeRange(op.source) << " → " << describeRange(op.target);
            } else if constexpr (std::is_same_v<T, reconform::ReconformDelete>) {
              stream << "Delete " << describeRange(op.target);
            } else if constexpr (std::is_same_v<T, reconform::ReconformRetime>) {
              stream << "Retime " << describeRange(op.target) << " → "
                     << formatSeconds(op.retimed_duration_seconds) << "s";
            }
          },
          operation.data);
      if (!operation.note.empty())
        stream << " — " << operation.note;
      stream << '\n';
    }

    reconformSummary_ = stream.str();
    refreshStatusBox();
  }

  void resized() override {
    auto area = getLocalBounds().reduced(16);
    header_.setBounds(area.removeFromTop(40));
    disclaimer_.setBounds(area.removeFromTop(24));
    statusBox_.setBounds(area);
  }

private:
  void refreshStatusBox() {
    juce::String text = sessionSummary_;
    if (text.isEmpty())
      text = initialInstructions();

    if (reconformSummary_.isNotEmpty()) {
      if (!text.isEmpty() && !text.endsWithChar('\n'))
        text << '\n';
      text << '\n' << reconformSummary_;
    }

    statusBox_.setText(text, juce::dontSendNotification);
  }

  static juce::String initialInstructions() {
    juce::String text;
    text << juce::String::fromUTF8(
        "Open an Orpheus session JSON via File \xe2\x86\x92 Open Session.\n");
    text << juce::String::fromUTF8(
        "Then use Session \xe2\x86\x92 Trigger ClipGrid Scene before rendering.\n");
    text << juce::String::fromUTF8(
        "Session \xe2\x86\x92 Render WAV writes synthetic stems into a directory.\n");
    text << "No DAW or plug-ins are required for this demonstration.";
    return text;
  }

  juce::Label header_;
  juce::Label disclaimer_;
  juce::TextEditor statusBox_;
  juce::AudioDeviceManager audioDeviceManager_;
  juce::String sessionSummary_;
  juce::String reconformSummary_;
};

class MainWindow : public juce::DocumentWindow, public juce::MenuBarModel {
public:
  explicit MainWindow(bool initialiseAudioDevice = true)
      : juce::DocumentWindow("Orpheus Demo Host", juce::Colours::darkgrey,
                             DocumentWindow::allButtons) {
    setUsingNativeTitleBar(true);
    setResizable(true, false);
    setContentOwned(new MainComponent(initialiseAudioDevice), true);
    setMenuBar(this);
    centreWithSize(640, 420);
    setVisible(true);
  }

  ~MainWindow() override {
    setMenuBar(nullptr);
  }

  void closeButtonPressed() override {
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
  }

  juce::StringArray getMenuBarNames() override {
    return {"File", "Session", "Help"};
  }

  juce::PopupMenu getMenuForIndex(int, const juce::String& menuName) override {
    juce::PopupMenu menu;
    if (menuName == "File") {
      menu.addItem(CommandIDs::openSession, "Open Session...");
      menu.addItem(CommandIDs::openReconformPlan, "Open Reconform Plan...");
      menu.addSeparator();
      menu.addItem(CommandIDs::quit, "Quit");
    } else if (menuName == "Session") {
      menu.addItem(CommandIDs::triggerClipgrid, "Trigger ClipGrid Scene", controller_.hasSession(),
                   controller_.hasSession() && !controller_.clipgridCommitted());
      menu.addItem(CommandIDs::renderSession,
                   juce::String::fromUTF8("Render WAV Stems\xe2\x80\xa6"),
                   controller_.clipgridCommitted());
    } else if (menuName == "Help") {
      menu.addItem(CommandIDs::about, "About Orpheus Demo Host");
    }
    return menu;
  }

  void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override {
    switch (menuItemID) {
    case CommandIDs::openSession:
      handleOpenSession();
      break;
    case CommandIDs::openReconformPlan:
      handleOpenReconformPlan();
      break;
    case CommandIDs::triggerClipgrid:
      handleTriggerClipgrid();
      break;
    case CommandIDs::renderSession:
      handleRender();
      break;
    case CommandIDs::quit:
      closeButtonPressed();
      break;
    case CommandIDs::about:
      juce::AlertWindow::showMessageBoxAsync(
          juce::AlertWindow::InfoIcon, "About Orpheus Demo Host",
          "A minimal, unbranded host demonstrating the Treefall SDK."
          "\n\nOpen a session, trigger the ClipGrid, then render synthetic stems to disk.");
      break;
    default:
      break;
    }
  }

  // Finite message-thread UI/model exercise; caller constructs this window without audio devices.
  bool runSelfSmoke(const juce::File& sessionFile, const juce::File& outputDirectory,
                    const juce::File& reconformFile, juce::String& error) {
    const auto require = [&](bool condition, const char* message) {
      if (!condition && error.isEmpty())
        error = message;
      return condition;
    };
    if (!require(!controller_.triggerClipgrid(error), "Commit accepted without a session") ||
        !require(error == "Load a session first", "Missing load-before-commit error"))
      return false;
    error.clear();
    if (!require(!controller_.renderToDirectory(outputDirectory, error),
                 "Render accepted without a session") ||
        !require(error == "Load a session first", "Missing load-before-render error"))
      return false;
    error.clear();
    if (!loadSessionForUI(sessionFile, error))
      return false;
    if (!require(!controller_.renderToDirectory(outputDirectory, error),
                 "Render accepted before commit") ||
        !require(error == "Trigger the ClipGrid scene before rendering",
                 "Missing commit-before-render error"))
      return false;
    error.clear();
    if (!commitClipgridForUI(error) || !renderToDirectory(outputDirectory, error))
      return false;
    const auto before = controller_.snapshot();
    const auto uiBefore = mainComponent().statusText();
    if (!require(before.clipgridCommitted && uiBefore.contains("ClipGrid committed: yes") &&
                     uiBefore.contains(outputDirectory.getFullPathName()),
                 "Committed render state missing from UI"))
      return false;

    juce::TemporaryFile invalidSession(".json");
    if (!require(invalidSession.getFile().replaceWithText("{"),
                 "Unable to write invalid session smoke input"))
      return false;
    if (!require(!loadSessionForUI(invalidSession.getFile(), error),
                 "Malformed session import succeeded"))
      return false;
    error.clear();
    if (!require(controller_.snapshot() == before && mainComponent().statusText() == uiBefore,
                 "Failed import changed the current graph or UI"))
      return false;
    if (reconformFile != juce::File()) {
      if (!loadReconformForUI(reconformFile, error))
        return false;
      if (!require(mainComponent().statusText().contains("Reconform plan:") &&
                       mainComponent().statusText().contains(reconformFile.getFullPathName()),
                   "Reconform import missing from UI"))
        return false;
    }
    return true;
  }

private:
  enum CommandIDs {
    openSession = 1,
    openReconformPlan,
    triggerClipgrid,
    renderSession,
    quit,
    about
  };

  MainComponent& mainComponent() {
    return *static_cast<MainComponent*>(getContentComponent());
  }

  bool loadSessionForUI(const juce::File& file, juce::String& error) {
    if (!controller_.openSession(file, error))
      return false;
    mainComponent().updateSnapshot(controller_.snapshot());
    menuItemsChanged();
    return true;
  }

  bool commitClipgridForUI(juce::String& error) {
    if (!controller_.triggerClipgrid(error))
      return false;
    mainComponent().updateSnapshot(controller_.snapshot());
    menuItemsChanged();
    return true;
  }

  bool renderToDirectory(const juce::File& directory, juce::String& error) {
    if (!controller_.renderToDirectory(directory, error))
      return false;
    mainComponent().updateSnapshot(controller_.snapshot());
    menuItemsChanged();
    return true;
  }

  bool loadReconformForUI(const juce::File& file, juce::String& error) {
    try {
      const auto plan = reconform::LoadReconformPlanFromFile(file.getFullPathName().toStdString());
      mainComponent().updateReconformPlan(file.getFullPathName(), plan);
      return true;
    } catch (const std::exception& ex) {
      error = juce::String("Failed to open reconform plan:\n") + ex.what();
      return false;
    }
  }

  void handleOpenSession() {
    juce::FileChooser chooser("Open Orpheus Session", juce::File(), "*.json;*.orp;*.*");
    if (!chooser.browseForFileToOpen())
      return;

    juce::String error;
    if (!loadSessionForUI(chooser.getResult(), error)) {
      juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Session Load", error);
      return;
    }
  }

  void handleOpenReconformPlan() {
    juce::FileChooser chooser("Open Reconform Plan", juce::File(), "*.json;*.*");
    if (!chooser.browseForFileToOpen())
      return;

    juce::String error;
    if (!loadReconformForUI(chooser.getResult(), error))
      juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Reconform Plan",
                                             error);
  }

  void handleTriggerClipgrid() {
    juce::String error;
    if (!commitClipgridForUI(error)) {
      juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "ClipGrid", error);
      return;
    }
  }

  void handleRender() {
    juce::FileChooser chooser("Render stems to directory", juce::File(), "*");
    if (!chooser.browseForDirectory())
      return;

    juce::String error;
    if (!renderToDirectory(chooser.getResult(), error)) {
      juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Render", error);
      return;
    }

    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Render Complete",
                                           "Synthetic stems were written to:\n" +
                                               chooser.getResult().getFullPathName());
  }

  OrpheusSessionController controller_{};
};

class DemoHostApplication : public juce::JUCEApplication {
public:
  const juce::String getApplicationName() override {
    return "Orpheus Demo Host";
  }
  const juce::String getApplicationVersion() override {
    return "0.1.0";
  }
  bool moreThanOneInstanceAllowed() override {
    return true;
  }

  void initialise(const juce::String& commandLine) override {
    juce::StringArray arguments;
    arguments.addTokens(commandLine, true);
    arguments.trim();
    arguments.removeEmptyStrings();
    for (auto& argument : arguments)
      argument = argument.unquoted();
    const bool selfSmoke = !arguments.isEmpty() && arguments[0] == "--self-smoke";
    if (selfSmoke && arguments.size() != 3 && arguments.size() != 4) {
      std::cerr << "Usage: OrpheusDemoHost --self-smoke SESSION_JSON OUTPUT_DIRECTORY "
                   "[RECONFORM_JSON]\n";
      setApplicationReturnValue(2);
      quit();
      return;
    }
    mainWindow_ = std::make_unique<MainWindow>(!selfSmoke);
    if (selfSmoke) {
      juce::MessageManager::callAsync([this, arguments] {
        juce::String error;
        const bool passed = mainWindow_->runSelfSmoke(
            juce::File::getCurrentWorkingDirectory().getChildFile(arguments[1]),
            juce::File::getCurrentWorkingDirectory().getChildFile(arguments[2]),
            arguments.size() == 4
                ? juce::File::getCurrentWorkingDirectory().getChildFile(arguments[3])
                : juce::File(),
            error);
        if (passed)
          std::cout << "Demo UI/model self-smoke passed (no audio device opened).\n";
        else
          std::cerr << "Demo UI/model self-smoke failed: " << error << '\n';
        setApplicationReturnValue(passed ? 0 : 1);
        quit();
      });
    }
  }

  void shutdown() override {
    mainWindow_.reset();
  }

  void systemRequestedQuit() override {
    quit();
  }

private:
  std::unique_ptr<MainWindow> mainWindow_;
};

START_JUCE_APPLICATION(DemoHostApplication)
