# KHashTable

Compact hash-table building blocks and focused tests for Linux-oriented C
development.

## Components

| Component | Purpose |
| --- | --- |
| `htable/htable_list.h` | Hash-aware predecessor list for handling colliding entries |
| `htable/htable_table.h` | Caller-buffered SWAR bucket table with overflow lists |
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

The htable suite currently covers list operations, list-match iteration, and
basic index/statistics behavior. It does not validate the kernel module or
concurrent access to the table backend.
The standalone SWAR suite checks byte masks, first-match and count helpers,
and the 32-, 64-, and 128-bit variants, including the portable 128-bit
fallback.

## Performance benchmarks

Build and run both benchmark suites with:

```sh
make benchmark
```

The suites can also be built and run separately:

```sh
make htable-bench
./tests/htable-bench

make linux-hashtable-bench
./tests/linux-hashtable-bench
```

The htable benchmark exercises the high-level `hash_init`/`hash_add`/`hash_del`
API with the SWAR table backend. The Linux benchmark includes
`<linux/hashtable.h>` directly from the configured kernel-header tree and runs
as a userspace process; it does not load a kernel module. Set `KDIR` if the
matching prepared kernel headers are not available at the default path.

Both programs emit CSV rows for insertion, successful lookup, unsuccessful
lookup, and deletion at 1,000, 10,000, and 100,000 entries. They use 128
buckets, giving average occupancies of approximately 7.8, 78.1, and 781.3
entries per bucket. The custom table's nominal capacity is not a hard limit:
overflow lists hold additional entries.

`make clean` cleans both the Kbuild outputs and the generated test binaries.

## Usage terms

See the copyright and usage terms in the source-file headers.
