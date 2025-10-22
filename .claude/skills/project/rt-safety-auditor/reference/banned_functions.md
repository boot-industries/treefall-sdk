# Banned Functions for Real-Time Audio Code

**Purpose:** Comprehensive list of functions forbidden in Orpheus SDK audio threads
**Version:** 1.0
**Last Updated:** 2025-10-18

---

## Category 1: Heap Allocation

### C++ Operators

```cpp
new                    // Heap allocation
delete                 // Heap deallocation
new[]                  // Array heap allocation
delete[]               // Array heap deallocation
```

### C Memory Functions

```cpp
malloc()               // Heap allocation
calloc()               // Zero-initialized heap allocation
realloc()              // Resize heap allocation
free()                 // Heap deallocation
aligned_alloc()        // Aligned heap allocation
posix_memalign()       // POSIX aligned allocation
```

### C++ Smart Pointers

```cpp
std::make_shared()     // Allocates control block + object
std::make_unique()     // Allocates object
std::allocate_shared() // Allocates with custom allocator
std::shared_ptr::reset() // May deallocate
std::unique_ptr::reset() // Deallocates
```

### STL Container Methods (May Allocate)

```cpp
std::vector::push_back()      // May reallocate
std::vector::insert()         // May reallocate
std::vector::resize()         // May allocate
std::vector::emplace_back()   // May reallocate
std::deque::push_back()       // May allocate node
std::deque::push_front()      // May allocate node
std::list::push_back()        // Allocates node
std::map::insert()            // Allocates node
std::set::insert()            // Allocates node
std::unordered_map::insert()  // May allocate bucket
std::string::operator+()      // Allocates new string
std::string::append()         // May reallocate
std::string::substr()         // Allocates new string
```

---

## Category 2: Locking Primitives

### C++ Mutexes

```cpp
std::mutex                     // Mutual exclusion lock
std::recursive_mutex           // Recursive lock
std::timed_mutex               // Timed mutex
std::shared_mutex              // Read-write lock
std::mutex::lock()             // Blocks until acquired
std::mutex::try_lock()         // May block briefly
```

### C++ Lock Guards

```cpp
std::lock_guard                // RAII mutex lock
std::unique_lock               // Movable mutex lock
std::shared_lock               // Shared read lock
std::scoped_lock               // Multiple mutex lock
```

### POSIX Threads

```cpp
pthread_mutex_lock()           // POSIX mutex lock
pthread_mutex_trylock()        // POSIX try lock
pthread_mutex_unlock()         // POSIX unlock
pthread_rwlock_rdlock()        // Read-write lock (read)
pthread_rwlock_wrlock()        // Read-write lock (write)
pthread_spin_lock()            // Spinlock (wastes CPU)
pthread_spin_trylock()         // Try spinlock
```

### Condition Variables (Blocking)

```cpp
std::condition_variable::wait()      // Blocks
std::condition_variable::wait_for()  // Timed block
std::condition_variable::wait_until() // Timed block
pthread_cond_wait()                   // POSIX condition wait
pthread_cond_timedwait()              // POSIX timed wait
```

### Barriers and Latches (C++20, Blocking)

```cpp
std::barrier::arrive_and_wait()  // Blocks until all threads arrive
std::latch::wait()               // Blocks until count reaches zero
```

---

## Category 3: Blocking I/O

### Console Output

```cpp
std::cout             // Buffered output (locks)
std::cerr             // Unbuffered error output (locks)
std::clog             // Buffered log output (locks)
printf()              // C formatted output (locks)
fprintf()             // File formatted output (locks)
puts()                // String output (locks)
putchar()             // Character output (locks)
std::ostream::operator<<()  // Stream insertion (may lock)
```

### File I/O

```cpp
fopen()               // Open file (blocks on disk)
fclose()              // Close file (blocks on flush)
fread()               // Read file (blocks on disk)
fwrite()              // Write file (blocks on disk)
fflush()              // Flush file buffer (blocks)
std::ifstream         // Input file stream (blocks)
std::ofstream         // Output file stream (blocks)
std::fstream          // Bidirectional file stream (blocks)
```

### File System Operations

```cpp
std::filesystem::exists()     // Check file existence (syscall)
std::filesystem::create_directory() // Create directory
std::filesystem::remove()     // Remove file/directory
std::filesystem::copy()       // Copy file
stat()                        // POSIX file stat (syscall)
open()                        // POSIX file open
read()                        // POSIX file read
write()                       // POSIX file write
```

### Network I/O

```cpp
socket()              // Create socket
connect()             // Connect to server (blocks)
send()                // Send data (may block)
recv()                // Receive data (blocks)
accept()              // Accept connection (blocks)
```

---

## Category 4: Threading and Synchronization

### Thread Creation/Management

```cpp
std::thread           // Create new thread (allocates)
std::jthread          // C++20 joinable thread (allocates)
pthread_create()      // POSIX thread creation
std::thread::join()   // Wait for thread to finish (blocks)
std::thread::detach() // Detach thread (may allocate)
```

### Thread Sleep/Yield

```cpp
std::this_thread::sleep_for()      // Sleep for duration (blocks)
std::this_thread::sleep_until()    // Sleep until time (blocks)
std::this_thread::yield()          // Yield to other threads (may block)
usleep()                            // POSIX microsecond sleep
sleep()                             // POSIX second sleep
nanosleep()                         // POSIX nanosecond sleep
```

---

## Category 5: Time Functions (Use Sample Counts Instead)

### C++ Chrono

```cpp
std::chrono::system_clock::now()          // Wall-clock time
std::chrono::steady_clock::now()          // Monotonic time
std::chrono::high_resolution_clock::now() // High-res time
std::chrono::duration                     // Time duration (use samples)
```

### C Time Functions

```cpp
time()                // POSIX time (seconds since epoch)
gettimeofday()        // POSIX time with microseconds
clock_gettime()       // POSIX high-resolution time
```

**Note:** Use 64-bit sample counters instead: `std::atomic<uint64_t> sampleCount_;`

---

## Category 6: Exceptions

### Exception Throwing/Catching

```cpp
throw                 // Throw exception (allocates)
try/catch             // Exception handling (overhead)
std::exception        // Standard exception base
std::runtime_error    // Runtime exception (allocates)
```

**Rationale:** Exception throwing allocates memory for exception object and stack unwinding.

**Fix:** Use error codes, `std::optional`, or `std::expected` (C++23)

---

## Category 7: Dynamic Dispatch (Use With Caution)

### Virtual Functions

```cpp
virtual methods       // OK if inheritance depth is shallow
dynamic_cast<>()      // RTTI lookup (may be slow)
typeid()              // RTTI type info (may allocate)
```

**Note:** Virtual functions are generally acceptable if:

- Inheritance hierarchy is shallow (1-2 levels)
- Virtual call overhead is profiled and acceptable
- Called once per buffer, not per sample

---

## Category 8: Standard Library Algorithms (May Allocate)

### STL Algorithms

```cpp
std::sort()           // May allocate (use std::stable_sort with pre-allocated buffer)
std::stable_sort()    // May allocate temporary buffer
std::inplace_merge()  // May allocate
std::set_union()      // May allocate output
```

**Fix:** Use in-place algorithms or pre-allocated scratch buffers

---

## Category 9: String Operations

### String Functions

```cpp
strlen()              // Unbounded loop (use known lengths)
strcmp()              // Unbounded loop
strcpy()              // Unbounded loop
strcat()              // Unbounded loop + may allocate
std::string           // Nearly all methods may allocate
std::to_string()      // Allocates new string
std::stringstream     // Allocates buffer
```

**Fix:** Use fixed-size char arrays with `snprintf()` or `std::array<char, N>`

---

## Category 10: Miscellaneous Forbidden Operations

### System Calls

```cpp
system()              // Execute shell command (forks process)
fork()                // Create new process
exec()                // Execute program
popen()               // Open pipe to process
```

### Signal Handling

```cpp
signal()              // Install signal handler (may allocate)
raise()               // Send signal to self
```

### Locale Operations

```cpp
std::locale           // Locale operations (may allocate)
setlocale()           // Set C locale (may allocate)
```

---

## Detection Patterns (for Auditor)

### Regex Patterns for grep

```bash
# Heap allocation
\bnew\s+\w+\[?|\bdelete\s+\[?|\bmalloc\(|\bcalloc\(|\brealloc\(|\bfree\(

# Locking primitives
std::mutex|std::lock_guard|std::unique_lock|std::scoped_lock|pthread_mutex

# Console I/O
std::cout|std::cerr|std::clog|printf\(|fprintf\(|puts\(

# File I/O
fopen\(|fclose\(|fread\(|fwrite\(|std::ifstream|std::ofstream

# Threading
std::thread|pthread_create|sleep_for|sleep_until|usleep\(

# Exceptions
\bthrow\s+|try\s*\{|catch\s*\(

# STL container mutations that may allocate
push_back\(|push_front\(|insert\(|emplace\(|resize\(

# String operations
std::string|strlen\(|strcmp\(|strcpy\(|strcat\(|to_string\(
```

---

## Allowlist (False Positives)

Some patterns may appear in safe code:

### Safe Uses

```cpp
// Placement new on pre-allocated memory (advanced)
new (preAllocatedBuffer) MyClass();

// std::array (no allocation, compile-time size)
std::array<float, 1024> buffer;

// Pre-allocated std::vector (safe if never exceeds capacity)
buffer_.reserve(kMaxSize);  // In constructor
// ... in audio thread:
buffer_.resize(actualSize); // Safe if actualSize <= kMaxSize
```

### Annotations

Add comments to suppress false positives:

```cpp
// SAFE: pre-allocated buffer
// RT-SAFE: lock-free atomic operation
// NON-RT: UI thread only
```

---

## Version History

| Version | Date       | Changes                    |
| ------- | ---------- | -------------------------- |
| 1.0     | 2025-10-18 | Initial comprehensive list |

---

## References

- Orpheus SDK Real-Time Constraints (rt_constraints.md)
- JUCE Framework Real-Time Safety Guide
- Ross Bencina - Real-time audio programming 101
