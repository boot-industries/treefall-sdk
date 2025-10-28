# Sanitizer Output Formats

**Purpose:** Document sanitizer error formats for parsing
**Version:** 1.0

---

## AddressSanitizer (ASan)

### Error Header

```
==12345==ERROR: AddressSanitizer: <error-type> on address 0x...
```

### Error Types

- `heap-use-after-free` - Accessing freed memory
- `heap-buffer-overflow` - Buffer overflow
- `stack-buffer-overflow` - Stack overflow
- `global-buffer-overflow` - Global buffer overflow
- `use-after-return` - Using stack variable after function returns
- `use-after-scope` - Using variable after scope ends
- `double-free` - Freeing memory twice
- `memory-leak` - Memory not freed

### Stack Trace

```
    #0 0x... in function_name file.cpp:line
    #1 0x... in caller_function file.cpp:line
```

### Parsing Pattern

```regex
==\d+==ERROR: AddressSanitizer: ([\w-]+) on address (0x[0-9a-f]+)
```

---

## ThreadSanitizer (TSan)

### Data Race

```
==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Read of size 4 at 0x... by thread T1:
    #0 function_name file.cpp:line

  Previous write of size 4 at 0x... by main thread:
    #0 other_function file.cpp:line
```

### Parsing Pattern

```regex
WARNING: ThreadSanitizer: ([\w\s]+) \(pid=\d+\)
```

---

## UndefinedBehaviorSanitizer (UBSan)

### Integer Overflow

```
file.cpp:42:10: runtime error: signed integer overflow: 2147483647 + 1 cannot be represented in type 'int'
```

### Null Pointer Dereference

```
file.cpp:67:5: runtime error: member access within null pointer of type 'struct Foo'
```

### Parsing Pattern

```regex
([\w/]+\.cpp):(\d+):(\d+): runtime error: (.+)
```

---

## Clean Output

### ASan Clean

```
=================================================================
==12345==ERROR: LeakSanitizer: detected memory leaks

SUMMARY: AddressSanitizer: 0 byte(s) leaked in 0 allocation(s).
```

### TSan Clean

```
==================
ThreadSanitizer: no issues found.
```

### UBSan Clean

```
(No output = clean)
```
