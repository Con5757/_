# Standard-library implementation status

This table separates functions implemented by the first native-runtime expansion from
API ideas whose signatures are documented but whose runtime contract is not yet ready.
The `languages` branch is still in active development; only the first group below is
registered as executable native functions by the included compiler patch.

## Implemented in the included compiler/runtime patch

| Module | Functions | Notes |
|---|---|---|
| `io` | `out`, `in`, `nl`, `strout`, `charin`, `errout`, `flush` | Line input uses a reusable runtime-owned buffer. `io.out` writes a newline. |
| `file` | `open`, `create`, `close`, `read`, `write`, `delete`, `rename`, `gpath`, `gsize` | Line-oriented text API; handles must be closed. Read returns a per-handle temporary buffer. |
| `memory` | `escape` | Releases known heap-owned results; compile-time checks reject literals, scalar values, const values, and ordinary local string buffers. Allocation and resize are not yet exposed as safe language-level typed pointers. |
| `string` | `length`, `copy`, `connect`, `substr`, `slice`, `replace`, `up`, `low`, `trim`, `equals`, `contains`, `starts_with`, `ends_with` | Allocating functions return owned memory. `substr` is `(text,start,length)`; `slice` is `(text,start,end)` with end exclusive. `replace` replaces all non-overlapping matches and rejects an empty search string. |
| `math` | `abs`, `sqrt`, `min`, `max`, `round`, `rdown`, `rup`, `sin`, `tan`, `cos`, `asin`, `atan`, `acos`, `log`, `logx`, `exp`, `pi`, `clamp`, `sign`, `lerp` | Trig functions use radians. Negative `sqrt` returns -1 in this first contract. If `clamp` receives `low > high`, it returns the input unchanged. `lerp` does not clamp the interpolation parameter. |
| `random` | `seed`, `next`, `range`, `unit` | Uses C `rand()`/`srand()`; not cryptographically secure. `range` is inclusive and may have modulo bias; invalid bounds or intervals wider than RAND_MAX + 1 return -1. |
| `time` | `utime`, `ctime`, `utctime`, `timedif` | Epoch time is in seconds. The C-library text functions return shared buffers containing a trailing newline. `timedif(left,right)` returns `left - right`. |

### Memory ownership

`string.copy`, `string.connect`, `string.substr`, `string.slice`, and `string.replace` return
heap-owned text. Call `memory.escape(result)` when finished with that result. The compiler rejects
obvious invalid arguments such as string literals, scalars, const values, and ordinary local
buffers; the programmer must still pass only memory they own. Never free borrowed pointers or
stack storage. In-place string operations (`up`, `low`, `trim`) require writable, NUL-terminated
storage.

## Documented design targets, not enabled as working native calls

| Module | Candidate functions | Why implementation is deferred |
|---|---|---|
| `memory` | `alloc`, `zalloc`, `mresize`, `sizeof`, `mcopy`, `mmove`, `mset` | Needs a stable `void*`/typed-pointer model, allocation ownership, and type-size grammar. |
| `vector` | `alloc`, `push`, `insert`, `remove`, `get`, `set`, `length`, `capacity`, `clear`, `erase` | Generic element types, bounds behavior, and storage ownership are undecided. |
| `system` | `exit`, `gcode`, `wait`, `readenv`, `setenv`, `curdir`, `chadir`, `args` | Process argument representation and cross-platform environment/process semantics need a contract. |
| `time` | `sleep`, `format`, `parsetime` | Sleep precision and stable date parsing/formatting conventions are not defined. |
| `convert` | `toInt`, `toFp`, `parseInt`, `parseFp`, `toString`, `hex`, `bin` | Invalid-input, overflow, rounding, and ownership behavior need explicit result rules. |
| `thread` | `start`, `join`, `detach`, `current`, `yield` | Callable values/closures, argument transport, ownership, and shutdown are not defined. |
| `sync` | `mutex`, `lock`, `unlock`, `destroy`, `condition`, `wait`, `signal`, `broadcast` | Lock ownership, deadlock/timeouts, cancellation, and wakeup rules need a portable contract. |
| `regex` | `compile`, `test`, `find`, `replace`, `free` | The regex dialect, Unicode behavior, captures, escaping, and failure model are undecided. |
| `network` | `connect`, `listen`, `accept`, `send`, `receive`, `close`, `lastError` | Socket types, TLS, DNS, blocking, timeouts, and platform dependencies need a contract. |
| `encoding` | `utf8Valid`, `utf8Length`, `base64Encode`, `base64Decode`, `hexEncode`, `hexDecode` | Binary byte arrays need a length-bearing representation; text byte length and character count differ. |

The full public design catalogue lives in [`libSyntaxList._`](libSyntaxList._). The website
only advertises the implemented first slice as ready and labels the rest as planned.
