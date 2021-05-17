// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>
#include "extent_protocol.h" // TODO: delete it

#define DISK_SIZE  1024*1024*16 // bytes. namely 16 MB
#define BLOCK_SIZE 512          // bytes, because 1 block is 4Kb, or 512 B
#define BLOCK_NUM  (DISK_SIZE/BLOCK_SIZE) // = 32 * 1024 blocks

typedef uint32_t blockid_t;

// disk layer -----------------------------------------

class disk {
 private:
  unsigned char blocks[BLOCK_NUM][BLOCK_SIZE];

 public:
  disk();
  void read_block(uint32_t id, char *buf);
  void write_block(uint32_t id, const char *buf);
};

// block layer -----------------------------------------

typedef struct superblock {
  uint32_t size;
  uint32_t nblocks;
  uint32_t ninodes;
} superblock_t;

class block_manager {
 private:
  std::list <blockid_t> free_block_list;  // defined by Alexanderia
  disk *d;
  std::map <uint32_t, int> using_blocks;  // record whether a block is used or not

 public:
  block_manager();
  struct superblock sb;

  uint32_t alloc_block();
  void free_block(uint32_t id);
  void read_block(uint32_t id, char *buf);
  void write_block(uint32_t id, const char *buf);
};

// inode layer -----------------------------------------

#define INODE_NUM  1024

// Inodes per block.
#define IPB           1 // so 1 block, 1 inode
//(BLOCK_SIZE / sizeof(struct inode))

// Block containing inode i
#define IBLOCK(i, nblocks)     ((nblocks)/BPB + (i)/IPB + 3)

// Bitmap bits per block
#define BPB           (BLOCK_SIZE*8)  // one block could contain information of BPB blocks

// Block containing bit for block b
#define BBLOCK(b) ((b)/BPB + 2) // why + 2? because there are boot block and super block in the front

#define NDIRECT 100
#define NINDIRECT (BLOCK_SIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

typedef struct inode {
  short type; // directory or file
  unsigned int size;  // number of bytes?
  unsigned int atime; // access time
  unsigned int mtime; // modify time
  unsigned int ctime; // close time
  blockid_t blocks[NDIRECT+1];   // Data block addresses
} inode_t;

class inode_manager {
 private:
  block_manager *bm;
  std::list <uint32_t> free_inode_list; // written by Alexanderia
  std::map <uint32_t, bool> using_inodes; // bitmap for inode table
  struct inode* get_inode(uint32_t inum);
  void put_inode(uint32_t inum, struct inode *ino);
  // helping functions
  blockid_t inode_get_block_id( const inode_t * inode, const unsigned int index ) const;
  void inode_allocate_block( inode_t * inode, unsigned int index );
  void inode_free_block_id( inode_t * inode, unsigned int index );
  void alloc_inode_with_id_and_size( const uint32_t type, const uint32_t inum, const unsigned int size );

 public:
  inode_manager();
  uint32_t alloc_inode(uint32_t type);
  void free_inode(uint32_t inum);
  void read_file(uint32_t inum, char **buf, int *size);
  void write_file(uint32_t inum, const char *buf, int size);
  void remove_file(uint32_t inum);
  void getattr(uint32_t inum, extent_protocol::attr &a);
};

#endif

