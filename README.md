# KHashTable

Compact hash-table building blocks and focused tests for Linux-oriented C
development.

## Components

| Component | Purpose |
| --- | --- |
| `htable/htable_list.h` | Hash-aware predecessor list for handling colliding entries |
| `htable/htable_table.h` | RCU-aware wrapper around Linux `rhashtable` |
| `htable/htable_multitable.h` | Filtered, multi-level table with configurable sizing |
| `htable/swar*.h` | SWAR byte matching and CRC32C helpers for 32-, 64-, and 128-bit values |
| `tests/` | List, table, iterator-macro, and SWAR tests |

## Build and run the tests

The userspace tests use GNU C11 and are compiled with SSE4.2 enabled for the
CRC32C intrinsics. Run them from the repository root:

```sh
make htable-test swar-test
./htable-test/htable-test
./tests/swar-test
```

To force both test binaries to rebuild:

```sh
make -B htable-test swar-test
```

The htable suite covers list operations, hash-match iteration, the
`htable_for_each` family of macros, and multi-level table collisions. Its
userspace table-iterator checks use a small lookup stub; they do not replace
testing the kernel `rhashtable` and RCU behavior in a kernel environment.
The standalone SWAR suite checks byte masks, first-match and count helpers,
and the 32-, 64-, and 128-bit variants, including the portable 128-bit
fallback.

`make clean` cleans both the Kbuild outputs and the generated test binaries.

## Usage terms

See the copyright and usage terms in the source-file headers.
