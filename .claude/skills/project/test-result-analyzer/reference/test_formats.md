# Test Output Formats

**Purpose:** Document recognized test output formats for parsing
**Version:** 1.0

---

## ctest Output Format

### Summary Line

```
X% tests passed, Y tests failed out of Z
```

### Individual Test Results

```
Start 1: TestName
1/47 Test #1: TestName .........................   Passed    0.12 sec
```

### Failed Test

```
Start 2: FailingTest
2/47 Test #2: FailingTest ......................***Failed    0.05 sec
```

---

## Google Test Output Format

### Success

```
[==========] Running 5 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 5 tests from TransportTests
[ RUN      ] TransportTests.PlayStop
[       OK ] TransportTests.PlayStop (10 ms)
[==========] 5 tests from 1 test suite ran. (50 ms total)
[  PASSED  ] 5 tests.
```

### Failure

```
[ RUN      ] AudioMixerTests.MixStereo
src/modules/m3/audio_mixer_test.cpp:42: Failure
Expected equality of these values:
  result
    Which is: 0.4999847
  0.5
[  FAILED  ] AudioMixerTests.MixStereo (10 ms)
```

---

## Parsing Patterns

### ctest Summary

```regex
(\d+)% tests passed, (\d+) tests failed out of (\d+)
```

### Individual Test

```regex
(\d+)/(\d+) Test #\d+: (.+?)\s+\.*\s+(Passed|Failed|\*\*\*Failed)\s+(\d+\.\d+) sec
```

### Google Test Failure

```regex
\[\s+FAILED\s+\]\s+(.+?)\s+\((\d+)\s+ms\)
```

### Error Location

```regex
([\w/]+\.cpp):(\d+): (Failure|Error)
```
