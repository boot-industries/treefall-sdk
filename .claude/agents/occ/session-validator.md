# Agent: Session Validator

## Purpose

Validate Clip Composer session JSON files for correctness, completeness, and backward compatibility. Detects schema violations, missing fields, and structural issues.

## Triggers

- Session file fails to load
- Need to verify backward compatibility after schema changes
- Creating test fixtures for integration tests
- User reports corrupted session
- User explicitly requests: "Validate session file" or "Check session JSON"

## Process

### 1. Load Session File

```bash
cat [session_file.json]
```

Verify:

- Valid JSON syntax (parseable)
- File encoding (UTF-8)
- File size reasonable (<10MB for typical sessions)

### 2. Schema Validation

#### Root Structure

```json
{
  "sessionMetadata": { ... },
  "clips": [ ... ],
  "routing": { ... }
}
```

**Required fields:**

- `sessionMetadata` (object)
- `clips` (array)
- `routing` (object, optional for backward compatibility)

#### Session Metadata

```json
{
  "name": "string",
  "version": "string (semantic version)",
  "createdDate": "ISO 8601 timestamp",
  "modifiedDate": "ISO 8601 timestamp",
  "sampleRate": number (e.g., 48000)
}
```

**Validations:**

- `name`: Non-empty string
- `version`: Matches `\d+\.\d+\.\d+` format
- `sampleRate`: 44100, 48000, or 96000
- Timestamps: Valid ISO 8601 format

#### Clip Data (per clip in `clips` array)

```json
{
  "handle": number,
  "name": "string",
  "filePath": "string (absolute path)",
  "buttonIndex": number (0-119),
  "tabIndex": number (0-7),
  "clipGroup": number (0-3),
  "sampleRate": number,
  "numChannels": number,
  "durationSamples": number (int64),
  "color": "string (hex color #RRGGBB)",
  "trimInSamples": number (int64, optional),
  "trimOutSamples": number (int64, optional),
  "fadeInSeconds": number (float, optional),
  "fadeOutSeconds": number (float, optional),
  "fadeInCurve": "string (optional)",
  "fadeOutCurve": "string (optional)"
}
```

**Validations:**

- `handle`: Unique, positive integer
- `buttonIndex`: 0-119 (120 buttons per tab)
- `tabIndex`: 0-7 (8 tabs)
- `clipGroup`: 0-3 (4 groups)
- `sampleRate`: 44100, 48000, or 96000
- `numChannels`: 1-32
- `durationSamples`: Positive int64
- `color`: Valid hex color format
- `trimInSamples`: 0 ≤ trimIn < durationSamples
- `trimOutSamples`: trimIn < trimOut ≤ durationSamples
- `fadeInSeconds`: 0.0-3.0
- `fadeOutSeconds`: 0.0-3.0
- `fadeInCurve`: "Linear", "EqualPower", or "Exponential"
- `fadeOutCurve`: "Linear", "EqualPower", or "Exponential"
- `filePath`: Check if file exists (warn if missing, don't fail)

#### Routing (optional, v0.1.0+ feature)

```json
{
  "clipGroups": [
    {"name": "string", "gain": number, "mute": boolean}
  ]
}
```

**Validations:**

- `clipGroups`: Array of 4 objects (if present)
- `gain`: -96.0 to +12.0 dB
- `mute`: Boolean

### 3. Cross-Field Validation

- **Unique handles:** No duplicate clip handles
- **Valid button assignments:** No two clips on same tab/button
- **Trim consistency:** trimOut > trimIn for all clips
- **Fade consistency:** fadeIn + fadeOut ≤ duration (warn if excessive)
- **File existence:** Warn for missing files, don't fail validation

### 4. Backward Compatibility Check

**v0.1.0-alpha schema changes:**

- Added 6 fields: `trimInSamples`, `trimOutSamples`, `fadeInSeconds`, `fadeOutSeconds`, `fadeInCurve`, `fadeOutCurve`
- These fields are optional (use `hasProperty()` checks in code)

**Validation:**

- If fields missing: "Session is v0.0.x format (pre-alpha). Will load with default values."
- If fields present: "Session is v0.1.0+ format. All metadata present."

### 5. Report Results

**Format:**

```
Session Validation Report
=========================
File: [session_file.json]
Size: [SIZE] KB

SUMMARY:
✅ Valid JSON syntax
✅ Schema compliant
✅ [N] clips loaded
⚠️  [N] warnings

DETAILS:
- Session version: 1.0.0
- Sample rate: 48000 Hz
- Clips: 12 total
  - Tab 0: 8 clips
  - Tab 1: 4 clips
- Routing: 4 clip groups configured

WARNINGS:
⚠️  Clip "Intro Music" (button 5): File not found at [path]
⚠️  Clip "Outro" (button 10): Fade times exceed 50% of duration (may cause overlap)

ERRORS:
❌ None

RECOMMENDATION: [Session is valid / Fix [N] errors before loading]
```

## Success Criteria

- [x] JSON syntax validated
- [x] Schema structure validated
- [x] All required fields present
- [x] Field values within valid ranges
- [x] Cross-field constraints satisfied
- [x] Backward compatibility verified
- [x] Clear report generated (errors, warnings, recommendations)

## Tools Required

- Bash (cat, jq for JSON parsing)
- Read (for reading session files)
- Grep (for searching patterns)

## Error Handling

### Invalid JSON Syntax

**Symptom:** JSON parse error (unexpected token, etc.)
**Action:**

1. Report exact line/column of syntax error
2. Show snippet of problematic JSON
3. Suggest: "Fix JSON syntax using validator like jsonlint.com"

### Missing Required Fields

**Symptom:** `sessionMetadata` or `clips` missing
**Action:**

1. Report: "Required field [field] missing from session"
2. Show expected schema structure
3. Suggest: "Session may be corrupted. Compare with valid session template."

### Invalid Field Values

**Symptom:** `buttonIndex` = 150 (out of range 0-119)
**Action:**

1. Report: "Clip [name]: buttonIndex=150 exceeds maximum 119"
2. Suggest: "Assign to valid button (0-119) or different tab"

### File Path Issues

**Symptom:** Audio file doesn't exist at specified path
**Action:**

1. Warn: "Clip [name]: Audio file not found at [path]"
2. Check if relative vs absolute path issue
3. Suggest: "Update filePath or restore missing audio file"

### Backward Compatibility Concerns

**Symptom:** Old session format (pre-v0.1.0)
**Action:**

1. Report: "Session uses v0.0.x format (missing trim/fade fields)"
2. Explain: "Will load with default values (no trim, no fade)"
3. Suggest: "Re-save session in Clip Composer v0.1.0+ to upgrade format"

---

## Example Usage

**User:** "This session file won't load. Can you check what's wrong?"

**Agent Response:**

1. Reads session file
2. Validates JSON syntax: ✅ Valid
3. Validates schema: ✅ Compliant
4. Cross-field validation: ❌ Found issue
5. Reports: "Error: Clip 'Intro Music' has trimOutSamples (500000) > durationSamples (480000). Trim point exceeds file length. Suggestion: Adjust trimOutSamples to ≤480000 or reload audio file."

---

## Test Fixtures

### Valid Session Template

**Location:** `tests/fixtures/valid_session.json`
**Purpose:** Baseline for validation testing

### Invalid Session Examples

**Location:** `tests/fixtures/invalid_*.json`
**Examples:**

- `invalid_syntax.json` - Malformed JSON
- `invalid_missing_clips.json` - Missing `clips` array
- `invalid_range.json` - Out-of-range values
- `invalid_trim.json` - trimOut < trimIn

---

**Last Updated:** October 22, 2025
**Version:** 1.0
**Schema Version:** v0.1.0-alpha (with trim/fade fields)
**Compatible with:** v0.1.0-alpha and later
