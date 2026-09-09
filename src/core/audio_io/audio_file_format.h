// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/audio_file_reader.h>
#include <orpheus/audio_file_writer.h>

#if defined(ORPHEUS_AUDIO_FILE_CAPABILITIES_HAVE_SNDFILE)
#include <sndfile.h>
namespace orpheus {
int sndfileFormatFor(AudioFileFormat format, AudioSampleFormat sample_format) noexcept;
AudioFileFormat audioFileFormatFromSndfile(int format) noexcept;
uint16_t audioFileBitDepthFromSndfile(int format) noexcept;
const char* audioFileCodecFromSndfile(int format) noexcept;
AudioFileMetadata audioFileMetadataFromSndfile(const SF_INFO& info);
} // namespace orpheus
#endif
