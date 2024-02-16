#include "bitmap.h"
#include "inode.h"
#include "blocks.h"

// prints inode metadata
void print_inode(inode_t *node) {
  printf("Inode location: %p\n", node);
  printf("Reference count: %d\n", node->refs);
  printf("Node permissions and type: %d\n", node->mode);
  printf("Size: %d\n", node->size);

  int i = 0;
  while (i < sizeof(node->ptrs)) {
    printf("Pointer %d: %d\n", i, node->ptrs[i]);
    i++;
  }

  printf("Indirect pointer: %d", node->iptr);
}

// returns pointer to an inode given inum
inode_t *get_inode(int inum) {
  inode_t *n = get_inode_bitmap() + 32;
  return &n[inum];
}

// allocates new inode onto filesystem
// returns index of new inode
int alloc_inode() {
  void *bm = get_inode_bitmap();
  int index;
  int i = 0;

  while (i < BLOCK_COUNT) {
    if (!bitmap_get(bm, i)) {
      bitmap_put(bm, i, 1); // makr in use
      inode_t *n = get_inode(i);
      // initialize
      n->refs = 1;
      n->mode = 0;
      n->size = 0;
      n->ptrs[0] = alloc_block();
      n->iptr = 0;
      return i;
    }

    ++i;
  }
  return -1;
}

// marks inode_t as free in the bitmap, clears the pointer locations
void free_inode(int inum) {
  inode_t *n = get_inode(inum);

  shrink_inode(n, 0);
  free_block(n->ptrs[0]);
  bitmap_put((void *) get_inode_bitmap(), inum, 0);
}

// increases size of given inode
// if too large, allocates new block
int grow_inode(inode_t *node, int size) {
  // calc number of blocks currently used by inode
  int num = node->size / BLOCK_SIZE;

  // alloc a new block if needed
  if (num == 0) {
    node->ptrs[0] = alloc_block();
  } else if (num == 1) {
    node->ptrs[1] = alloc_block();
  }

  // update size of the inode
  node->size = size;
  return 0;
}

// decreases size of inode
int shrink_inode(inode_t *node, int size) {
  // calc number of blocks currently used by inode
  int num = node->size / BLOCK_SIZE;

  // free last block if exists
  if (num > 0) {
    int bn = inode_get_bnum(node, node->size);
    free_block(bn);
  }

  // update size of the inode
  node->size = size;
  return 0;
}

// gets block number for given inum
int inode_get_bnum(inode_t *node, int fbnum) {
  int b = fbnum / BLOCK_SIZE;

  if (b == 0) {
    return node->ptrs[0];
  } else if (b == 1) {
    return node->ptrs[1];
  } else {
    int *iptr = blocks_get_block(node->iptr);
    return iptr[b - 2];
  }
}






