// FUSE driver for NUFS
// based on cs3650 starter code

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "storage.h"
#include "slist.h"

// checks if file exists
int nufs_access(const char *path, int mask) {
  int rv = storage_stat(path, NULL);
  printf("access(%s, %04o) -> %d\n", path, mask, rv);
  return rv;
}

// gets file attributes
int nufs_getattr(const char *path, struct stat *st) {
  int rv = storage_stat(path, st);
  printf("getattr(%s) -> (%d) {mode: %04o, size: %ld}\n", path, rv,
         st ? st->st_mode : 0, st ? st->st_size : 0);
  return rv;
}

// lists directory contents
int nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi) {
  struct stat st;

  int rv = nufs_getattr(path, &st);
  if (rv < 0) {
    return rv;
  }
  filler(buf, ".", &st, 0);

  // add parent entry
  if (strcmp(path, "/") == 0) {
    filler(buf, "..", &st, 0);
  } else {
    char parent_path[256];
    strncpy(parent_path, path, sizeof(parent_path) - 1);
    char *last_slash = strrchr(parent_path, '/');
    if (last_slash == parent_path) {
      strcpy(parent_path, "/");
    } else if (last_slash) {
      *last_slash = '\0';
    }

    rv = nufs_getattr(parent_path, &st);
    if (rv == 0) {
      filler(buf, "..", &st, 0);
    }
  }

  slist_t *list = storage_list(path);
  for (slist_t *curr = list; curr != NULL; curr = curr->next) {
    char full_path[256];

    if (strcmp(path, "/") == 0) {
      snprintf(full_path, sizeof(full_path), "/%s", curr->data);
    } else {
      snprintf(full_path, sizeof(full_path), "%s/%s", path, curr->data);
    }

    rv = nufs_getattr(full_path, &st);
    if (rv == 0) {
      filler(buf, curr->data, &st, 0);
    }
  }

  if (list != NULL) {
    slist_free(list);
  }

  printf("readdir(%s) -> %d\n", path, 0);
  return 0;
}

int nufs_mknod(const char *path, mode_t mode, dev_t rdev) {
  int rv = storage_mknod(path, mode);
  printf("mknod(%s, %04o) -> %d\n", path, mode, rv);
  return rv;
}

int nufs_mkdir(const char *path, mode_t mode) {
  int rv = storage_mknod(path, mode | 040000);
  printf("mkdir(%s, %04o) -> %d\n", path, mode, rv);
  return rv;
}

int nufs_unlink(const char *path) {
  int rv = storage_unlink(path);
  printf("unlink(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_link(const char *from, const char *to) {
  int rv = storage_link(from, to);
  printf("link(%s => %s) -> %d\n", from, to, rv);
  return rv;
}

int nufs_rmdir(const char *path) {
  int rv = storage_unlink(path);
  printf("rmdir(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_rename(const char *from, const char *to) {
  int rv = storage_rename(from, to);
  printf("rename(%s => %s) -> %d\n", from, to, rv);
  return rv;
}

int nufs_chmod(const char *path, mode_t mode) {
  int rv = 0;
  printf("chmod(%s, %04o) -> %d\n", path, mode, rv);
  return rv;
}

int nufs_truncate(const char *path, off_t size) {
  int rv = storage_truncate(path, size);
  printf("truncate(%s, %ld bytes) -> %d\n", path, size, rv);
  return rv;
}

int nufs_open(const char *path, struct fuse_file_info *fi) {
  int rv = 0;
  printf("open(%s) -> %d\n", path, rv);
  return rv;
}

int nufs_read(const char *path, char *buf, size_t size, off_t offset,
              struct fuse_file_info *fi) {
  int rv = storage_read(path, buf, size, offset);
  printf("read(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
  return rv;
}

int nufs_write(const char *path, const char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
  int rv = storage_write(path, buf, size, offset);
  printf("write(%s, %ld bytes, @+%ld) -> %d\n", path, size, offset, rv);
  return rv;
}

int nufs_utimens(const char *path, const struct timespec ts[2]) {
  int rv = storage_set_time(path, ts);
  printf("utimens(%s, [%ld, %ld; %ld %ld]) -> %d\n", path, ts[0].tv_sec,
         ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec, rv);
  return rv;
}

int nufs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
               unsigned int flags, void *data) {
  int rv = -ENOTTY;
  printf("ioctl(%s, %d, ...) -> %d\n", path, cmd, rv);
  return rv;
}

void nufs_init_ops(struct fuse_operations *ops) {
  memset(ops, 0, sizeof(struct fuse_operations));
  ops->access = nufs_access;
  ops->getattr = nufs_getattr;
  ops->readdir = nufs_readdir;
  ops->mknod = nufs_mknod;
  ops->mkdir = nufs_mkdir;
  ops->link = nufs_link;
  ops->unlink = nufs_unlink;
  ops->rmdir = nufs_rmdir;
  ops->rename = nufs_rename;
  ops->chmod = nufs_chmod;
  ops->truncate = nufs_truncate;
  ops->open = nufs_open;
  ops->read = nufs_read;
  ops->write = nufs_write;
  ops->utimens = nufs_utimens;
  ops->ioctl = nufs_ioctl;
}

struct fuse_operations nufs_ops;

int main(int argc, char *argv[]) {
  assert(argc > 2 && argc < 6);

  storage_init(argv[--argc]);

  nufs_init_ops(&nufs_ops);
  return fuse_main(argc, argv, &nufs_ops, NULL);
}
