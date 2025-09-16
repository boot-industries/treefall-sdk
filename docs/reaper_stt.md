# Speech-to-text helper (`reaper_stt`)

The `reaper_stt` extension transcribes `PCM_source_transfer_t` blocks, inserts
project markers for each recognized word, and maintains an in-memory text lane
so other extensions can query or edit the recognized words.

## Exported API

`reaper_stt` registers the following helpers via `plugin_register`:

| `GetFunc` key        | Signature                                      | Description |
| -------------------- | ---------------------------------------------- | ----------- |
| `TranscribeSource`   | `void (*)(PCM_source *src)`                    | Runs the speech-to-text pass on the supplied source and repopulates the internal text lane. |
| `STT_FindWord`       | `int (*)(const char *word)`                    | Returns the first index of `word` within the text lane, or `-1` when the word is not present. |
| `STT_ReplaceWord`    | `void (*)(const char *oldWord, const char *newWord)` | Replaces every instance of `oldWord` in the lane with `newWord`. |
| `STT_SetEngine`      | `void (*)(STTEngine *engine)`                  | Installs a custom transcription engine. Pass `nullptr` to restore the default stub. |

The names above map to the corresponding `API_*` registrations (for example,
`API_TranscribeSource`). Use `rec->GetFunc()` or `plugin_getapi()` with the
`GetFunc` key to retrieve each function pointer.

## Using the helpers from another extension

```c++
#include "reaper_plugin.h"
#include "reaper_plugin_functions.h"
#include "stt_engine.h"

using TranscribeSourceFn = void (*)(PCM_source *);
using STTFindWordFn = int (*)(const char *);
using STTReplaceWordFn = void (*)(const char *, const char *);
using STTSetEngineFn = void (*)(STTEngine *);

TranscribeSourceFn transcribe =
  reinterpret_cast<TranscribeSourceFn>(rec->GetFunc("TranscribeSource"));
STTFindWordFn find_word =
  reinterpret_cast<STTFindWordFn>(rec->GetFunc("STT_FindWord"));
STTReplaceWordFn replace_word =
  reinterpret_cast<STTReplaceWordFn>(rec->GetFunc("STT_ReplaceWord"));
STTSetEngineFn set_engine =
  reinterpret_cast<STTSetEngineFn>(rec->GetFunc("STT_SetEngine"));
```

Load `reaper_plugin_functions.h` (via `REAPERAPI_LoadAPI`) before calling the
helpers. `TranscribeSource` must be called before `STT_FindWord` or
`STT_ReplaceWord` to populate the internal lane.

## Supplying a custom engine

`STT_SetEngine` accepts implementations of the `STTEngine` interface defined in
`stt_engine.h`. The helper stores the pointer you provide and uses it for all
subsequent `TranscribeSource` calls. Passing `nullptr` reverts to the built-in
stub engine, and the extension automatically resets to the stub when it unloads.
