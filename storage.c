/**
 * Storage implementation - high level file operations.
 */
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>

#include "storage.h"
#include "inode.h"
#include "directory.h"
#include "blocks.h"

int directory_lookup_path(const char *path);

//initialize storage system and create root directory
void storage_init(const char *path) {
  blocks_init(path);
  directory_init();
}

// Get file metadata
int storage_stat(const char *path, struct stat *st) {
  int inum = directory_lookup_path(path);
  if (inum < 0) {
    return inum;
  }

  inode_t *node = get_inode(inum);

  if (st != NULL) {
    memset(st, 0, sizeof(struct stat));
    st->st_mode = node->mode;
    st->st_size = node->size;
    st->st_nlink = node->refs;
    st->st_uid = getuid();
  }
  return 0;
}

//Read data from file
int storage_read(const char *path, char *buf, size_t size, off_t offset) {
  int inum = directory_lookup_path(path);
  if (inum < 0) {
    return inum;
  }

  inode_t *node = get_inode(inum);

  if (offset >= node->size) {
    return 0;
  }

  if (offset + size > node->size) {
    size = node->size - offset;
  }

  if (node->block == -1) {
    return 0;
  }

  void *block_data = blocks_get_block(node->block);
  memcpy(buf, block_data + offset, size);

  return size;
}

//write data to file
int storage_write(const char *path, const char *buf, size_t size, off_t offset) {
  int inum = directory_lookup_path(path);
  if (inum < 0) {
    return inum;
  }

  inode_t *node = get_inode(inum);

  // Allocate block if file needs to grow
  int new_size = offset + size;
  if (new_size > node->size) {
    int rv = grow_inode(node, new_size);
    if (rv < 0) {
      return rv;
    }
    node->size = new_size;
  }

  if (node->block == -1) {
    return -EIO;
  }

  void *block_data = blocks_get_block(node->block);
  memcpy(block_data + offset, buf, size);

  return size;
}

//extend file to specified size
int storage_truncate(const char *path, off_t size) {
  int inum = directory_lookup_path(path);
  if (inum < 0) {
    return inum;
  }

  inode_t *node = get_inode(inum);

  if (size < node->size) {
    shrink_inode(node, size);
  } else if (size > node->size) {
    int rv = grow_inode(node, size);
    if (rv < 0) {
      return rv;
    }
  }

  node->size = size;
  return 0;
}

// Creates new file or directory
int storage_mknod(const char *path, int mode) {
  if (directory_lookup_path(path) >= 0) {
    return -EEXIST;
  }

  // Find parent directory
  char *path_copy = strdup(path);
  char *dir_path = dirname(path_copy);
  int parent_inum = directory_lookup_path(dir_path);
  free(path_copy);

  if (parent_inum < 0) {
    return -ENOENT;
  }

  int inum = alloc_inode();
  if (inum < 0) {
    return -ENOSPC;
  }

  inode_t *node = get_inode(inum);
  node->mode = mode;
  node->size = 0;
  node->refs = 1;

  // Add entry to parent directory
  path_copy = strdup(path);
  char *file_name = basename(path_copy);
  inode_t *parent = get_inode(parent_inum);
  int rv = directory_put(parent, file_name, inum);
  free(path_copy);

  if (rv < 0) {
    free_inode(inum);
  }

  return rv;
}

// Deletes file and frees its inode
int storage_unlink(const char *path) {
  char *path_copy = strdup(path);
  char *dir_path = dirname(path_copy);
  int parent_inum = directory_lookup_path(dir_path);
  free(path_copy);

  if (parent_inum < 0) {
    return -ENOENT;
  }

  // Need to copy filename since basename returns pointer into path_copy
  path_copy = strdup(path);
  char *file_name_temp = basename(path_copy);
  char *file_name = strdup(file_name_temp);

  inode_t *parent = get_inode(parent_inum);
  int inum = directory_lookup(parent, file_name);
  free(path_copy);

  if (inum < 0) {
    free(file_name);
    return inum;
  }

  inode_t *node = get_inode(inum);
  node->refs--;

  if (node->refs == 0) {
    shrink_inode(node, 0);
    free_inode(inum);
  }

  int rv = directory_delete(parent, file_name);
  free(file_name);
  return rv;
}

int storage_link(const char *from, const char *to) {
  int inum = directory_lookup_path(from);
  if (inum < 0) {
    return inum;
  }

  char *path_copy = strdup(to);
  char *dir_path = dirname(path_copy);
  int parent_inum = directory_lookup_path(dir_path);
  free(path_copy);

  if (parent_inum < 0) {
    return -ENOENT;
  }

  path_copy = strdup(to);
  char *file_name = basename(path_copy);

  inode_t *parent = get_inode(parent_inum);
  inode_t *node = get_inode(inum);
  node->refs++;

  int rv = directory_put(parent, file_name, inum);
  free(path_copy);

  return rv;
}

//rename file
int storage_rename(const char *from, const char *to) {
  int rv = storage_link(from, to);
  if (rv < 0) {
    return rv;
  }
  return storage_unlink(from);
}

//set file access/modification times
int storage_set_time(const char *path, const struct timespec ts[2]) {
  return 0;
}

//List directory
slist_t *storage_list(const char *path) {
  return directory_list(path);
}
