#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  blockid_t result = *free_blocks_number.begin();
  free_blocks_number.erase(free_blocks_number.begin());
  return result;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  free_blocks_number.insert(id);
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

  // add all blocks to free block set
  for (int i = IBLOCK(INODE_NUM - 1, BLOCK_NUM) + 1; i < BLOCK_NUM; ++i) {
    free_blocks_number.insert(i);
  }
}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  for (uint32_t i = 1; i<INODE_NUM; ++i) {
    free_inode_num.insert(i);
  }
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  struct inode* ino = (struct inode*)malloc(sizeof(struct inode));
  ino->type = type;
  ino->atime = time(NULL);
  ino->ctime = time(NULL);
  ino->mtime = time(NULL);
  ino->size = 0;
  uint32_t inum = *free_inode_num.begin();
  free_inode_num.erase(free_inode_num.begin());
  put_inode(inum, ino);
  free(ino);
  return inum;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */

  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  struct inode* target = get_inode(inum);
  if (target == NULL)
    return;
  int file_size = target->size;
  char* copy_buf = (char*)malloc(file_size);
  *buf_out = copy_buf;
  *size = file_size;
  if (file_size <= NDIRECT * BLOCK_SIZE) {
    // no need to use indirect block
    int copied = 0;
    int inode_blocks_index = 0;
    while (copied + BLOCK_SIZE <= file_size) {
      char local_buf[BLOCK_SIZE];
      bm->read_block(target->blocks[inode_blocks_index], local_buf);
      memcpy(copy_buf + inode_blocks_index * BLOCK_SIZE, local_buf, BLOCK_SIZE);
      inode_blocks_index++;
      copied += BLOCK_SIZE;
    }
    if (copied < file_size) {
      char local_buf[BLOCK_SIZE];
      bm->read_block(target->blocks[inode_blocks_index], local_buf);
      memcpy(copy_buf + inode_blocks_index * BLOCK_SIZE, local_buf, file_size - copied);
      return;
    }
    return;
  }
  else {
    // indirect block is used
    int copied = 0;
    int inode_blocks_index = 0;
    while (copied + BLOCK_SIZE <= file_size) {
      char local_buf[BLOCK_SIZE];
      get_datablock(target, inode_blocks_index, local_buf);
      memcpy(copy_buf + inode_blocks_index * BLOCK_SIZE, local_buf, BLOCK_SIZE);
      inode_blocks_index++;
      copied += BLOCK_SIZE;
    }
    if (copied < file_size) {
      char local_buf[BLOCK_SIZE];
      get_datablock(target, inode_blocks_index, local_buf);
      memcpy(copy_buf + inode_blocks_index * BLOCK_SIZE, local_buf, file_size - copied);
      return;
    }
    return;
  }
  return;
}

void
inode_manager::get_datablock(struct inode* inode, int inode_blocks_index, char* buf) {
  if (inode_blocks_index >= 0 && inode_blocks_index < NDIRECT) {
    bm->read_block(inode->blocks[inode_blocks_index], buf);
    return;
  }
  else if (inode_blocks_index >= NDIRECT && inode_blocks_index < NDIRECT + NINDIRECT) {
    char local_buf[BLOCK_SIZE];
    bm->read_block(inode->blocks[NDIRECT], local_buf);
    uint32_t* data_blocks_number = (uint32_t*) local_buf;
    inode_blocks_index -= NDIRECT;
    bm->read_block(data_blocks_number[inode_blocks_index], buf);
    return;
  }
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  struct inode* target = get_inode(inum);
  if (target == NULL)
    return;
  a.atime = target->atime;
  a.ctime = target->ctime;
  a.mtime = target->mtime;
  a.size = target->size;
  a.type = target->type;
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  
  return;
}
