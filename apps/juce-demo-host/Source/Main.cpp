// SPDX-License-Identifier: MIT

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "orpheus/abi.h"
#include "orpheus/errors.h"
#include "json_io.h"
#include "otio/reconform_plan.h"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <type_traits>
#include <vector>

#if JUCE_WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;
using orpheus::core::SessionGraph;
namespace session_json = orpheus::core::session_json;
namespace reconform = orpheus::core::otio;

namespace {

juce::String DescribeLoaderError();

juce::String StatusToString(orpheus_status status)
{
    return juce::String(orpheus_status_to_string(status));
}

fs::path ExecutableDirectory()
{
    const auto execFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    return execFile.getParentDirectory().getFullPathName().toStdString();
}

#if JUCE_WINDOWS
using ModuleHandle = HMODULE;
ModuleHandle LoadModule(const fs::path& path, juce::String& error)
{
    juce::String pathString(path.string());
    ModuleHandle handle = ::LoadLibraryW((LPCWSTR)pathString.toWideCharPointer());
    if (handle == nullptr)
    {
        const juce::String message = DescribeLoaderError();
        error = juce::String("LoadLibrary failed for ") + pathString +
                (message.isNotEmpty() ? juce::String(": ") + message : juce::String());
    }
    return handle;
}
void UnloadModule(ModuleHandle handle)
{
    if (handle != nullptr)
        ::FreeLibrary(handle);
}
void* LoadSymbol(ModuleHandle handle, const char* name)
{
    return reinterpret_cast<void*>(::GetProcAddress(handle, name));
}
#else
using ModuleHandle = void*;
ModuleHandle LoadModule(const fs::path& path, juce::String& error)
{
    ModuleHandle handle = ::dlopen(path.c_str(), RTLD_NOW);
    if (handle == nullptr)
        error = juce::String("dlopen failed: ") + juce::String(::dlerror());
    return handle;
}
void UnloadModule(ModuleHandle handle)
{
    if (handle != nullptr)
        ::dlclose(handle);
}
void* LoadSymbol(ModuleHandle handle, const char* name)
{
    ::dlerror();
    return ::dlsym(handle, name);
}
#endif

juce::String PlatformLibraryName(const juce::String& stem)
{
#if JUCE_WINDOWS
    return stem + ".dll";
#elif JUCE_MAC
    return "lib" + stem + ".dylib";
#else
    return "lib" + stem + ".so";
#endif
}

struct ModuleEntry
{
    juce::String stem;
    const char* entryPoint;
};

constexpr ModuleEntry kModules[] = {
    { "orpheus_session", "orpheus_session_abi_v1" },
    { "orpheus_clipgrid", "orpheus_clipgrid_abi_v1" },
    { "orpheus_render", "orpheus_render_abi_v1" },
};

juce::String DescribeLoaderError()
{
#if JUCE_WINDOWS
    const DWORD err = ::GetLastError();
    if (err == 0)
        return {};
    LPWSTR buffer = nullptr;
    const DWORD length = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    juce::String message;
    if (length != 0 && buffer != nullptr)
    {
        message = juce::String(buffer).trim();
        ::LocalFree(buffer);
    }
    else
    {
        message = juce::String((int)err);
    }
    return message;
#else
    if (const char* msg = ::dlerror())
        return juce::String(msg);
    return {};
#endif
}

} // namespace

class OrpheusModuleLoader
{
public:
    struct Tables
    {
        const orpheus_session_api_v1* session{};
        const orpheus_clipgrid_api_v1* clipgrid{};
        const orpheus_render_api_v1* render{};
    };

    ~OrpheusModuleLoader() { unload(); }

    bool ensureLoaded(juce::String& error)
    {
        if (tables_.session != nullptr && tables_.clipgrid != nullptr && tables_.render != nullptr)
            return true;

        if (const char* overrideDir = std::getenv("ORPHEUS_DEMO_HOST_LIBDIR"))
        {
            if (overrideDir[0] != '\0')
            {
                const fs::path overridePath(overrideDir);
                if (loadFrom(overridePath, error))
                    return true;
                error = juce::String("Failed to load Orpheus modules from override directory: ") +
                        overridePath.string() + "\n" + error;
                return false;
            }
        }

        const fs::path execDir = ExecutableDirectory();
        juce::String directError;
        if (loadFrom(execDir, directError))
            return true;

        const fs::path parentDir = execDir.parent_path();
        if (!parentDir.empty())
        {
            const fs::path libDir = parentDir / "lib";
            if (libDir != execDir)
            {
                juce::String fallbackError;
                if (loadFrom(libDir, fallbackError))
                    return true;
                error = juce::String("Failed to load Orpheus modules from ") + execDir.string() +
                        "\n" + directError + "\nFallback to " + libDir.string() + " failed:\n" +
                        fallbackError;
                return false;
            }
        }

        error = juce::String("Failed to load Orpheus modules from ") + execDir.string() +
                "\n" + directError;
        return false;
    }

    bool loadFrom(const fs::path& directory, juce::String& error)
    {
        unload();

        for (const auto& module : kModules)
        {
            ModuleHandle handle{};
            const auto fileName = PlatformLibraryName(module.stem);
            const auto modulePath = directory / fileName.toStdString();
            juce::String moduleError;
            handle = LoadModule(modulePath, moduleError);
            if (handle == nullptr)
            {
                error = juce::String("Unable to load ") + modulePath.string();
                if (moduleError.isNotEmpty())
                    error += ": " + moduleError;
                unload();
                return false;
            }

            modules_.push_back(handle);

            auto* symbol = LoadSymbol(handle, module.entryPoint);
            if (symbol == nullptr)
            {
                const juce::String symbolError = DescribeLoaderError();
                error = juce::String("Missing entry point ") + module.entryPoint +
                        (symbolError.isNotEmpty() ? juce::String(": ") + symbolError : juce::String());
                unload();
                return false;
            }

            if (module.stem == "orpheus_session")
            {
                auto fn = reinterpret_cast<const orpheus_session_api_v1*(*) (uint32_t, uint32_t*, uint32_t*)>(symbol);
                uint32_t major{};
                uint32_t minor{};
                tables_.session = fn(ORPHEUS_ABI_MAJOR, &major, &minor);
                if (tables_.session == nullptr || major != ORPHEUS_ABI_MAJOR)
                {
                    error = "Session ABI negotiation failed";
                    unload();
                    return false;
                }
            }
            else if (module.stem == "orpheus_clipgrid")
            {
                auto fn = reinterpret_cast<const orpheus_clipgrid_api_v1*(*) (uint32_t, uint32_t*, uint32_t*)>(symbol);
                uint32_t major{};
                uint32_t minor{};
                tables_.clipgrid = fn(ORPHEUS_ABI_MAJOR, &major, &minor);
                if (tables_.clipgrid == nullptr || major != ORPHEUS_ABI_MAJOR)
                {
                    error = "ClipGrid ABI negotiation failed";
                    unload();
                    return false;
                }
            }
            else if (module.stem == "orpheus_render")
            {
                auto fn = reinterpret_cast<const orpheus_render_api_v1*(*) (uint32_t, uint32_t*, uint32_t*)>(symbol);
                uint32_t major{};
                uint32_t minor{};
                tables_.render = fn(ORPHEUS_ABI_MAJOR, &major, &minor);
                if (tables_.render == nullptr || major != ORPHEUS_ABI_MAJOR)
                {
                    error = "Render ABI negotiation failed";
                    unload();
                    return false;
                }
            }
        }

        return tables_.session != nullptr && tables_.clipgrid != nullptr && tables_.render != nullptr;
    }

    void unload()
    {
        tables_ = {};
        for (auto handle : modules_)
            UnloadModule(handle);
        modules_.clear();
    }

    [[nodiscard]] const Tables& tables() const noexcept { return tables_; }

private:
    Tables tables_{};
    std::vector<ModuleHandle> modules_{};
};

class OrpheusSessionController
{
public:
    struct Snapshot
    {
        juce::String sessionName;
        juce::String sourcePath;
        std::size_t trackCount{};
        std::size_t clipCount{};
        double tempoBpm{};
        double rangeStart{};
        double rangeEnd{};
        bool clipgridCommitted{};
        juce::String lastRenderDirectory;
    };

    explicit OrpheusSessionController(OrpheusModuleLoader& loader) : loader_(loader) {}
    ~OrpheusSessionController() { reset(); }

    bool openSession(const juce::File& file, juce::String& error)
    {
        if (!loader_.ensureLoaded(error))
            return false;

        SessionGraph parsed;
        try
        {
            parsed = session_json::LoadSessionFromFile(file.getFullPathName().toStdString());
        }
        catch (const std::exception& ex)
        {
            error = juce::String("Session load failed: ") + ex.what();
            return false;
        }

        const auto* sessionApi = loader_.tables().session;
        const auto* clipgridApi = loader_.tables().clipgrid;
        if (sessionApi == nullptr || clipgridApi == nullptr)
        {
            error = "ABI tables unavailable";
            return false;
        }

        struct HandleGuard
        {
            const orpheus_session_api_v1* api{};
            orpheus_session_handle handle{};
            ~HandleGuard()
            {
                if (api != nullptr && handle != nullptr)
                    api->destroy(handle);
            }
        } guard{ sessionApi, nullptr };

        auto status = sessionApi->create(&guard.handle);
        if (status != ORPHEUS_STATUS_OK)
        {
            error = juce::String("Session create failed: ") + StatusToString(status);
            return false;
        }

        auto* sessionImpl = reinterpret_cast<SessionGraph*>(guard.handle);
        sessionImpl->set_name(parsed.name());
        sessionImpl->set_render_sample_rate(parsed.render_sample_rate());
        sessionImpl->set_render_bit_depth(parsed.render_bit_depth());
        sessionImpl->set_render_dither(parsed.render_dither());
        sessionImpl->set_session_range(parsed.session_start_beats(), parsed.session_end_beats());

        status = sessionApi->set_tempo(guard.handle, parsed.tempo());
        if (status != ORPHEUS_STATUS_OK)
        {
            error = juce::String("Tempo apply failed: ") + StatusToString(status);
            return false;
        }

        std::vector<TrackState> newTracks;
        std::size_t newClipCount = 0;
        newTracks.reserve(parsed.tracks().size());

        for (const auto& trackPtr : parsed.tracks())
        {
            orpheus_track_handle trackHandle{};
            const orpheus_track_desc trackDesc{ trackPtr->name().c_str() };
            status = sessionApi->add_track(guard.handle, &trackDesc, &trackHandle);
            if (status != ORPHEUS_STATUS_OK)
            {
                error = juce::String("Track add failed: ") + StatusToString(status);
                return false;
            }

            TrackState trackState;
            trackState.handle = trackHandle;
            for (const auto& clipPtr : trackPtr->clips())
            {
                const orpheus_clip_desc clipDesc{ clipPtr->name().c_str(), clipPtr->start(), clipPtr->length(), 0u };
                orpheus_clip_handle clipHandle{};
                status = clipgridApi->add_clip(guard.handle, trackHandle, &clipDesc, &clipHandle);
                if (status != ORPHEUS_STATUS_OK)
                {
                    error = juce::String("Clip add failed: ") + StatusToString(status);
                    return false;
                }
                trackState.clips.push_back(clipHandle);
                ++newClipCount;
            }
            newTracks.push_back(std::move(trackState));
        }

        reset();

        graph_ = std::move(parsed);
        sessionHandle_ = guard.handle;
        guard.handle = nullptr;
        tracks_ = std::move(newTracks);
        clipCount_ = newClipCount;
        sourcePath_ = file.getFullPathName();
        clipgridCommitted_ = false;
        lastRenderDirectory_.clear();

        return true;
    }

    bool triggerClipgrid(juce::String& error)
    {
        if (sessionHandle_ == nullptr)
        {
            error = "Load a session first";
            return false;
        }
        if (clipgridCommitted_)
            return true;

        const auto* clipgridApi = loader_.tables().clipgrid;
        if (clipgridApi == nullptr)
        {
            error = "ClipGrid ABI unavailable";
            return false;
        }

        const auto status = clipgridApi->commit(sessionHandle_);
        if (status != ORPHEUS_STATUS_OK)
        {
            error = juce::String("ClipGrid commit failed: ") + StatusToString(status);
            return false;
        }

        clipgridCommitted_ = true;
        return true;
    }

    bool renderToDirectory(const juce::File& directory, juce::String& error)
    {
        if (sessionHandle_ == nullptr)
        {
            error = "Load a session first";
            return false;
        }
        if (!clipgridCommitted_)
        {
            error = "Trigger the ClipGrid scene before rendering";
            return false;
        }

        const auto* renderApi = loader_.tables().render;
        if (renderApi == nullptr)
        {
            error = "Render ABI unavailable";
            return false;
        }

        if (!directory.exists())
            directory.createDirectory();

        const std::string directoryPath = directory.getFullPathName().toStdString();
        const auto status = renderApi->render_tracks(sessionHandle_, directoryPath.c_str());
        if (status != ORPHEUS_STATUS_OK)
        {
            error = juce::String("Render failed: ") + StatusToString(status);
            return false;
        }

        lastRenderDirectory_ = directory.getFullPathName();
        return true;
    }

    [[nodiscard]] bool hasSession() const noexcept { return sessionHandle_ != nullptr; }
    [[nodiscard]] bool clipgridCommitted() const noexcept { return clipgridCommitted_; }

    Snapshot snapshot() const
    {
        Snapshot snap{};
        snap.sessionName = graph_.name();
        snap.sourcePath = sourcePath_;
        snap.trackCount = graph_.tracks().size();
        snap.clipCount = clipCount_;
        snap.tempoBpm = graph_.tempo();
        snap.rangeStart = graph_.session_start_beats();
        snap.rangeEnd = graph_.session_end_beats();
        snap.clipgridCommitted = clipgridCommitted_;
        snap.lastRenderDirectory = lastRenderDirectory_;
        return snap;
    }

private:
    struct TrackState
    {
        orpheus_track_handle handle{};
        std::vector<orpheus_clip_handle> clips;
    };

    void reset()
    {
        if (sessionHandle_ != nullptr)
        {
            if (const auto* sessionApi = loader_.tables().session)
                sessionApi->destroy(sessionHandle_);
        }
        sessionHandle_ = nullptr;
        tracks_.clear();
        clipCount_ = 0;
        graph_ = SessionGraph{};
        sourcePath_.clear();
        clipgridCommitted_ = false;
        lastRenderDirectory_.clear();
    }

    OrpheusModuleLoader& loader_;
    SessionGraph graph_{};
    std::vector<TrackState> tracks_{};
    std::size_t clipCount_{};
    orpheus_session_handle sessionHandle_{};
    juce::String sourcePath_;
    bool clipgridCommitted_{};
    juce::String lastRenderDirectory_;
};

class MainComponent : public juce::Component
{
public:
    MainComponent()
    {
        addAndMakeVisible(header_);
        header_.setText("Orpheus Demo Host", juce::dontSendNotification);
        header_.setFont({ 24.0f, juce::Font::bold });

        addAndMakeVisible(disclaimer_);
        disclaimer_.setJustificationType(juce::Justification::centredLeft);
        disclaimer_.setColour(juce::Label::textColourId, juce::Colours::orange);
        disclaimer_.setText("Evaluation build – renders synthetic stems only.", juce::dontSendNotification);

        addAndMakeVisible(statusBox_);
        statusBox_.setReadOnly(true);
        statusBox_.setMultiLine(true);
        statusBox_.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
        statusBox_.setColour(juce::TextEditor::textColourId, juce::Colours::lightgreen);
        statusBox_.setFont({ 14.0f });
        statusBox_.setText(initialInstructions(), juce::dontSendNotification);

        audioDeviceManager_.initialise(0, 2, nullptr, true);
    }

    void updateSnapshot(const OrpheusSessionController::Snapshot& snapshot)
    {
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

    void updateReconformPlan(const juce::String& planPath,
                             const reconform::ReconformPlan& plan)
    {
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

        for (std::size_t index = 0; index < plan.operations.size(); ++index)
        {
            const auto& operation = plan.operations[index];
            stream << "  [" << (index + 1) << "] ";
            std::visit(
                [&](const auto& op) {
                    using T = std::decay_t<decltype(op)>;
                    if constexpr (std::is_same_v<T, reconform::ReconformInsert>)
                    {
                        stream << "Insert " << describeRange(op.source) << " → "
                               << describeRange(op.target);
                    }
                    else if constexpr (std::is_same_v<T, reconform::ReconformDelete>)
                    {
                        stream << "Delete " << describeRange(op.target);
                    }
                    else if constexpr (std::is_same_v<T, reconform::ReconformRetime>)
                    {
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

    void resized() override
    {
        auto area = getLocalBounds().reduced(16);
        header_.setBounds(area.removeFromTop(40));
        disclaimer_.setBounds(area.removeFromTop(24));
        statusBox_.setBounds(area);
    }

private:
    void refreshStatusBox()
    {
        juce::String text = sessionSummary_;
        if (text.isEmpty())
            text = initialInstructions();

        if (reconformSummary_.isNotEmpty())
        {
            if (!text.isEmpty() && !text.endsWithChar('\n'))
                text << '\n';
            text << '\n' << reconformSummary_;
        }

        statusBox_.setText(text, juce::dontSendNotification);
    }

    static juce::String initialInstructions()
    {
        juce::String text;
        text << "Open an Orpheus session JSON via File → Open Session.\n";
        text << "Then use Session → Trigger ClipGrid Scene before rendering.\n";
        text << "Session → Render WAV writes synthetic stems into a directory.\n";
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

class MainWindow : public juce::DocumentWindow, public juce::MenuBarModel
{
public:
    MainWindow() : juce::DocumentWindow("Orpheus Demo Host", juce::Colours::darkgrey, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        setContentOwned(new MainComponent(), true);
        setMenuBar(this);
        centreWithSize(640, 420);
        setVisible(true);
    }

    ~MainWindow() override
    {
        setMenuBar(nullptr);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    juce::StringArray getMenuBarNames() override
    {
        return { "File", "Session", "Help" };
    }

    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override
    {
        juce::PopupMenu menu;
        if (menuName == "File")
        {
            menu.addItem(CommandIDs::openSession, "Open Session...");
            menu.addItem(CommandIDs::openReconformPlan, "Open Reconform Plan...");
            menu.addSeparator();
            menu.addItem(CommandIDs::quit, "Quit");
        }
        else if (menuName == "Session")
        {
            menu.addItem(CommandIDs::triggerClipgrid, "Trigger ClipGrid Scene", controller_.hasSession(), controller_.hasSession() && !controller_.clipgridCommitted());
            menu.addItem(CommandIDs::renderSession, "Render WAV Stems…", controller_.clipgridCommitted());
        }
        else if (menuName == "Help")
        {
            menu.addItem(CommandIDs::about, "About Orpheus Demo Host");
        }
        return menu;
    }

    void menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/) override
    {
        switch (menuItemID)
        {
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
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                        "About Orpheus Demo Host",
                                                        "A minimal, unbranded host demonstrating the Orpheus SDK."\
                                                        "\n\nOpen a session, trigger the ClipGrid, then render synthetic stems to disk.");
                break;
            default:
                break;
        }
    }

private:
    enum CommandIDs
    {
        openSession = 1,
        openReconformPlan,
        triggerClipgrid,
        renderSession,
        quit,
        about
    };

    MainComponent& mainComponent()
    {
        return *static_cast<MainComponent*>(getContentComponent());
    }

    void handleOpenSession()
    {
        juce::FileChooser chooser("Open Orpheus Session", juce::File(), "*.json;*.orp;*.*");
        if (!chooser.browseForFileToOpen())
            return;

        juce::String error;
        if (!controller_.openSession(chooser.getResult(), error))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Session Load", error);
            return;
        }

        mainComponent().updateSnapshot(controller_.snapshot());
        menuItemsChanged();
    }

    void handleOpenReconformPlan()
    {
        juce::FileChooser chooser("Open Reconform Plan", juce::File(), "*.json;*.*");
        if (!chooser.browseForFileToOpen())
            return;

        const juce::File file = chooser.getResult();
        try
        {
            const reconform::ReconformPlan plan =
                reconform::LoadReconformPlanFromFile(file.getFullPathName().toStdString());
            mainComponent().updateReconformPlan(file.getFullPathName(), plan);
        }
        catch (const std::exception& ex)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Reconform Plan",
                                                   juce::String("Failed to open reconform plan:\n") + ex.what());
        }
    }

    void handleTriggerClipgrid()
    {
        juce::String error;
        if (!controller_.triggerClipgrid(error))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "ClipGrid", error);
            return;
        }

        mainComponent().updateSnapshot(controller_.snapshot());
        menuItemsChanged();
    }

    void handleRender()
    {
        juce::FileChooser chooser("Render stems to directory", juce::File(), "*");
        if (!chooser.browseForDirectory())
            return;

        juce::String error;
        if (!controller_.renderToDirectory(chooser.getResult(), error))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Render", error);
            return;
        }

        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "Render Complete",
                                               "Synthetic stems were written to:\n" + chooser.getResult().getFullPathName());
        mainComponent().updateSnapshot(controller_.snapshot());
        menuItemsChanged();
    }

    OrpheusModuleLoader loader_{};
    OrpheusSessionController controller_{ loader_ };
};

class DemoHostApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Orpheus Demo Host"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        mainWindow_ = std::make_unique<MainWindow>();
    }

    void shutdown() override
    {
        mainWindow_.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    std::unique_ptr<MainWindow> mainWindow_;
};

START_JUCE_APPLICATION(DemoHostApplication)
