/**
 * Directory manipulation functions.
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

#include "directory.h"
#include "inode.h"
#include "blocks.h"
#include "slist.h"
#include "bitmap.h"

int directory_lookup_path(const char *path);

// creates root directory if it doesn't exist
void directory_init() {
  void *ibm = get_inode_bitmap();

  if (!bitmap_get(ibm, 0)) {
    bitmap_put(ibm, 0, 1);

    inode_t *root = get_inode(0);
    root->mode = 040755;
    root->size = 0;
    root->refs = 2;
    root->block = -1;

    printf("+ directory_init() -> root created\n");
  }
}

// searches directory for entry with given name
int directory_lookup(inode_t *di, const char *name) {
  if (!di || di->size == 0 || di->block == -1) {
    return -ENOENT;
  }

  dirent_t *entries = (dirent_t *)blocks_get_block(di->block);
  int num_entries = di->size / sizeof(dirent_t);

  for (int ii = 0; ii < num_entries; ++ii) {
    if (strcmp(entries[ii].name, name) == 0) {
      return entries[ii].inum;
    }
  }

  return -ENOENT;
}

// adds new entry to directory
int directory_put(inode_t *di, const char *name, int inum) {
  assert(di != NULL);
  assert(strlen(name) < DIR_NAME_LENGTH);

  if (directory_lookup(di, name) >= 0) {
    return -EEXIST;
  }

  // allocate block for first entry
  if (di->block == -1) {
    int bnum = alloc_block();
    if (bnum < 0) {
      return -ENOSPC;
    }
    di->block = bnum;
    memset(blocks_get_block(bnum), 0, BLOCK_SIZE);
  }

  dirent_t *entries = (dirent_t *)blocks_get_block(di->block);
  int num_entries = di->size / sizeof(dirent_t);

  if ((num_entries + 1) * sizeof(dirent_t) > BLOCK_SIZE) {
    return -ENOSPC;
  }

  strncpy(entries[num_entries].name, name, DIR_NAME_LENGTH - 1);
  entries[num_entries].name[DIR_NAME_LENGTH - 1] = '\0';
  entries[num_entries].inum = inum;

  di->size += sizeof(dirent_t);

  printf("+ directory_put(%s -> inode %d)\n", name, inum);
  return 0;
}

// removes entry from directory
int directory_delete(inode_t *di, const char *name) {
  if (!di || di->size == 0 || di->block == -1) {
    return -ENOENT;
  }

  dirent_t *entries = (dirent_t *)blocks_get_block(di->block);
  int num_entries = di->size / sizeof(dirent_t);

  for (int ii = 0; ii < num_entries; ++ii) {
    if (strcmp(entries[ii].name, name) == 0) {
      // move last entry to this spot
      if (ii < num_entries - 1) {
        entries[ii] = entries[num_entries - 1];
      }

      di->size -= sizeof(dirent_t);
      printf("+ directory_delete(%s)\n", name);
      return 0;
    }
  }

  return -ENOENT;
}

// returns list of entries in directory
slist_t *directory_list(const char *path) {
  int inum = directory_lookup_path(path);
  if (inum < 0) {
    return NULL;
  }

  inode_t *di = get_inode(inum);
  if (di->size == 0 || di->block == -1) {
    return NULL;
  }

  dirent_t *entries = (dirent_t *)blocks_get_block(di->block);
  int num_entries = di->size / sizeof(dirent_t);

  slist_t *list = NULL;
  for (int ii = num_entries - 1; ii >= 0; --ii) {
    list = slist_cons(entries[ii].name, list);
  }

  return list;
}

//Print directory contents
void print_directory(inode_t *dd) {
  if (!dd) {
    printf("Directory: NULL\n");
    return;
  }

  printf("Directory contents (size=%d):\n", dd->size);

  if (dd->block == -1 || dd->size == 0) {
    return;
  }

  dirent_t *entries = (dirent_t *)blocks_get_block(dd->block);
  int num_entries = dd->size / sizeof(dirent_t);

  for (int ii = 0; ii < num_entries; ++ii) {
    printf("  %s -> inode %d\n", entries[ii].name, entries[ii].inum);
  }
}

// resolves full path to inode number by walking directory tree
int directory_lookup_path(const char *path) {
  if (strcmp(path, "/") == 0) {
    return 0;
  }

  int inum = 0;

  if (path[0] == '/') {
    path = path + 1;
  }

  if (path[0] == '\0') {
    return 0;
  }

  char *path_copy = strdup(path);
  char *token = strtok(path_copy, "/");

  while (token != NULL) {
    inode_t *dir = get_inode(inum);
    inum = directory_lookup(dir, token);

    if (inum < 0) {
      free(path_copy);
      return inum;
    }

    token = strtok(NULL, "/");
  }

  free(path_copy);
  return inum;
}
