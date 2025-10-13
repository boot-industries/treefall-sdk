/**
 * @file wasm_bindings.cpp
 * @brief Emscripten bindings for Orpheus SDK
 *
 * Minimal WASM interface exposing core Orpheus functionality to JavaScript.
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <string>
#include <memory>

using namespace emscripten;

// TODO: Replace with actual Orpheus SDK headers when linking
// For now, stub implementations for proof-of-concept

class SessionGraph {
public:
    SessionGraph() = default;

    bool loadFromJSON(const std::string& json) {
        // TODO: Implement actual JSON session loading
        return true;
    }

    double getTempo() const {
        return 120.0;
    }

    void setTempo(double bpm) {
        // TODO: Implement
    }
};

// Global session instance
static std::unique_ptr<SessionGraph> g_session;

/**
 * Get Orpheus SDK version
 * @return Version string in semver format
 */
std::string getVersion() {
    // TODO: Link to actual SDK version from orpheus/core.h
    return "0.1.0-wasm";
}

/**
 * Initialize Orpheus engine
 * @return true if initialization successful
 */
bool initialize() {
    g_session = std::make_unique<SessionGraph>();
    return true;
}

/**
 * Shutdown Orpheus engine
 */
void shutdown() {
    g_session.reset();
}

/**
 * Load session from JSON string
 * @param jsonString JSON session data
 * @return true if successful
 */
bool loadSession(const std::string& jsonString) {
    if (!g_session) {
        return false;
    }
    return g_session->loadFromJSON(jsonString);
}

/**
 * Render click track
 * @param bpm Beats per minute
 * @param bars Number of bars to render
 * @return JSON string with result or error
 */
std::string renderClick(double bpm, int bars) {
    if (!g_session) {
        return R"({"error": "Session not initialized"})";
    }

    // TODO: Implement actual click track rendering
    // For now, return success stub
    return R"({
        "success": true,
        "sampleRate": 48000,
        "channels": 2,
        "samples": 0,
        "duration": 0.0
    })";
}

/**
 * Get current session tempo
 * @return BPM value
 */
double getTempo() {
    if (!g_session) {
        return 0.0;
    }
    return g_session->getTempo();
}

/**
 * Set session tempo
 * @param bpm Beats per minute
 */
void setTempo(double bpm) {
    if (g_session) {
        g_session->setTempo(bpm);
    }
}

// Emscripten bindings
EMSCRIPTEN_BINDINGS(orpheus_module) {
    function("getVersion", &getVersion);
    function("initialize", &initialize);
    function("shutdown", &shutdown);
    function("loadSession", &loadSession);
    function("renderClick", &renderClick);
    function("getTempo", &getTempo);
    function("setTempo", &setTempo);
}
