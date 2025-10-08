# ORP-CDX-013 — CI Revalidation Matrix

This document captures the local verification steps used to revalidate fixes from ORP-CDX-007 through ORP-CDX-012 under the strict configuration matrix.

## Build Matrix

| Platform | Build Type | Flags |
| --- | --- | --- |
| Linux (GCC 13.3) | Release | `-Wall -Wextra -Werror` |
| Linux (GCC 13.3) | Debug w/ Sanitizers | `-Wall -Wextra -Werror -fsanitize=address,undefined` (propagated to compile and link)

> Windows (/W4 /WX /permissive-) and Clang variants are covered in CI; only the Linux legs were executed locally in this validation run.

## Release Build Verification

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS="-Wall -Wextra -Werror" \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Werror"
cmake --build build --parallel
```

### Targeted Tests

```
ctest -C Release -R conformance
ctest -C Release -R abi_link
ctest -C Release -R RenderTracksBasic
```

All tests passed with zero compiler warnings.

## Sanitized Debug Build Verification

```
cmake -S . -B build-sanitized -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-Wall -Wextra -Werror -fsanitize=address,undefined" \
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -Werror -fsanitize=address,undefined" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitized --parallel
```

### Targeted Tests

```
ctest -C Debug -R conformance
ctest -C Debug -R abi_link
ctest -C Debug -R RenderTracksBasic
```

All tests passed with sanitizers enabled and no warnings-as-errors triggered.

## Outstanding Issues

No residual warnings were observed in either configuration. The conformance, ABI link, and render track smoke suites are green under the stricter compiler and sanitizer settings.
