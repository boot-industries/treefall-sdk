# File Format Specification

## Audio Formats

### Primary Formats
- **Uncompressed Audio**
  * WAV (BWF compatible)
    - Security Requirements:
      * Format state protection
      * Resource allocation security
      * Processing chain validation
    - PCM: 16/24/32-bit
    - Float: 32/64-bit
  * AIFF
    - PCM: 16/24/32-bit
    - Float: 32/64-bit
  * RF64
    - Support for >4GB files
    - BWF metadata compatible

### Secondary Formats
- **Compressed Audio**
  * MP3 (import only)
    - All standard bitrates
    - ID3v2 metadata
  * AAC (import only)
    - All standard profiles
    - Container: M4A
  * FLAC
    - All bit depths
    - Import/Export support
  * OGG Vorbis (import only)
    - All quality levels

## Session Formats

### Project Bundle
- **Core Structure**
  * SQLite database
    - Clip metadata
    - Grid layout
    - Routing configuration
    - Automation data
  * Audio files
    - Original or consolidated
    - Preview cache
    - Backup references

### Configuration Files
- **System Settings**
  * Interface configuration
  * MIDI mappings
  * GPI/GPO setup
  * User preferences

- **Format**
  * JSON for readability
  * Binary for performance data
  * Version control support
  * Backup management

## Performance Requirements

### Import/Export
- **Loading Performance**
  * Instant preview (<100ms)
  * Background loading
  * Format detection
  * Error recovery

- **Export Performance**
  * Background processing
  * Progress reporting
  * Cancel support
  * Error handling

### Format Conversion
- **Real-time Processing**
  * Sample rate conversion
  * Bit depth adaptation
  * Channel mapping
  * Format transcoding

## Platform Integration

### Windows
- **Native Integration**
  * Shell integration
  * Format association
  * Thumbnail support
  * Quick preview

### macOS
- **Native Integration**
  * Finder integration
  * Quick Look support
  * Spotlight indexing
  * Format UTIs

### iOS
- **System Integration**
  * Files app support
  * Share sheet integration
  * Background import
  * iCloud support

### Session File Format
- **Metadata Storage**
  * Timestamp Data
    - Source configuration
      * Primary source selection
      * Backup source order
      * Source quality thresholds
    - Per-clip timing data
      * Start time records
      * Source switching history
      * Drift compensation logs
    - Format: ISO 8601 with microsecond precision 