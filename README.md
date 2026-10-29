# FUSE Filesystem

A custom filesystem in C, mounted through FUSE, built on a flat 1 MB disk image divided
into 4 KB blocks. CS3650 Computer Systems, Northeastern University.

The interesting part is not that `ls` works. It is that every POSIX call the kernel sends
has to be answered correctly from a byte array, with no filesystem underneath to fall back on.

## What it does

- **Inode-based storage.** Each file and directory has an inode holding mode, size, reference
  count, and block pointers. The disk image is laid out as a superblock, an inode bitmap, a
  block bitmap, the inode table, then data blocks.
- **Files larger than one block.** A file is not limited to 4 KB. Block pointers plus an
  indirect block let a file span many blocks, with `storage_truncate` growing and shrinking
  the allocation as the file changes size.
- **Directories as data.** A directory is a regular inode whose data blocks hold name-to-inode
  entries, so nested directories fall out of the same mechanism rather than needing a special case.
- **Hard links and rename.** `nufs_link` adds a directory entry pointing at an existing inode
  and bumps its reference count; `nufs_unlink` decrements and only frees blocks when the count
  reaches zero. `rename` moves an entry between directories without touching the data.
- **Full POSIX surface:** `access`, `getattr`, `readdir`, `mknod`, `mkdir`, `unlink`, `link`,
  `rmdir`, `rename`, `chmod`, `truncate`, `open`, `read`, `write`, `utimens`.

## Debugging

Memory-safety bugs here are brutal because a stray write corrupts the disk image rather than
crashing, so the failure shows up several operations later as a wrong file size or a lost
directory entry. GDB and Valgrind were the tools that made that tractable: Valgrind for
invalid reads and writes against the mapped image, GDB for stepping through the block
allocator when the bitmap and the inode table disagreed.

**All 31 tests in `test.pl` pass.**

## Build and run

```bash
make            # build nufs
make mount      # mount the filesystem at mnt/
make unmount    # unmount
make test       # run the test suite
```

## Layout

```
nufs.c        FUSE operation handlers, the driver
storage.c/.h  inode allocation, block allocation, read/write/truncate
directory.c/.h  directory entries, lookup, insert, delete
inode.h       inode structure
helpers/      bitmap and block access  (provided as course starter code)
hints/        starter scaffolding       (provided as course starter code)
test.pl       test suite
```

`helpers/` and `hints/` were provided with the assignment. Everything in `nufs.c`,
`storage.c`, and `directory.c` is my implementation.
