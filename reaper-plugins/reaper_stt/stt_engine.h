#pragma once

#include <string>
#include "../../sdk/reaper_plugin.h"

// Abstract interface for speech-to-text engines. Implementations can
// perform synchronous or asynchronous transcription.
class STTEngine {
public:
  virtual ~STTEngine() = default;
  virtual std::string Transcribe(const ReaSample *samples,
                                 int nch, int length,
                                 double samplerate) = 0;
};

// Set a custom STT engine. Passing nullptr restores the default stub
// implementation. The caller must ensure that the supplied engine
// remains valid for the duration of its use.
void STT_SetEngine(STTEngine *engine);

