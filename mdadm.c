#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "net.h"
#include "mdadm.h"
#include "jbod.h"
#include "cache.h"
int isMount = -1;
uint32_t encode_op(int cmd, int disk_num, int block_num)
{
  uint32_t op = 0;
  op |= (cmd << 26);
  op |= (disk_num << 22);
  op |= block_num;
  return op;
}
int mdadm_mount(void)
{
  if (isMount == 1)
    return -1;
  uint32_t op = encode_op(JBOD_MOUNT, 0, 0);
  int c = jbod_client_operation(op, NULL);
  if (c == -1)
    return -1;
  isMount = 1;
  return 1;
}
int mdadm_unmount(void)
{
  if (isMount == -1)
    return -1;
  uint32_t op = encode_op(JBOD_UNMOUNT, 0, 0);
  jbod_client_operation(op, NULL);
  isMount = -1;
  return 1;
}
int translate_bladdress(int ad)
{
  int block_num = (ad % 65536) / 256;
  return block_num;
}
int translate_daddress(int ad)
{
  int disk_num = ad / (65536);
  return disk_num;
}
int seekb(int adr)
{
  uint32_t block = encode_op(JBOD_SEEK_TO_BLOCK, 0, adr);
  return block;
}
int seekd(int adr)
{
  uint32_t disk = encode_op(JBOD_SEEK_TO_DISK, adr, 0);
  return disk;
}
int minimum(int a, int b)
{
  if (a < b)
  {
    return a;
  }
  else
  {
    return b;
  }
}
int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf)
{
  if (isMount == -1)
    return -1;
  uint32_t op = 0;
  if (addr + len > 1024 * 1024)
  {
    return -1;
  }
  if (len > 1024)
    return -1;
  if (buf == NULL && len)
    return -1;
  int num_read = 0;
  uint8_t mybuf[256];
  while (len > 0)
  {
    int offset = (addr % 256);
    int disknum = addr / 65536;
    int a = 0;
    int blocknum = (addr % 65536) / 256;
    if(cache_enabled())
      a = cache_lookup(disknum, blocknum, buf);

    if (a != 1)
    {
      op = seekd(disknum);
      jbod_client_operation(op, mybuf);
      op = seekb(blocknum);
      jbod_client_operation(op, mybuf);
      op = encode_op(JBOD_READ_BLOCK, 0, 0);
      jbod_client_operation(op, mybuf);
    }
    int num_bytes_to_read_from_block = minimum(len, minimum(256, 256 - offset));
    memcpy(buf + num_read, mybuf + offset, num_bytes_to_read_from_block);

    if(a!=1)
      cache_insert(disknum, blocknum, buf);

    num_read += num_bytes_to_read_from_block;
    len -= num_bytes_to_read_from_block;
    addr += num_bytes_to_read_from_block;
  }

  return num_read;
}

// write function
int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf)
{
  // checking invalid and out of bounds parameters.
  if (isMount == -1)
    return -1;
  uint32_t op = 0;
  if (addr + len > 1024 * 1024)
  {
    return -1;
  }
  if (len > 1024)
    return -1;
  if (buf == NULL && len)
    return -1;
  int num_write = 0;
  uint8_t mybuf[JBOD_BLOCK_SIZE];
  int num_read = 0;
  int i = 0;

  while (len > 0)
  {

    int disknum = addr / 65536;
    int blocknum = (addr % 65536) / 256;
    int offset = (addr % 256);
    int a=0;
    if(cache_enabled()){
      a = cache_lookup(disknum, blocknum, mybuf);
    }
    if (a != 1)
    {
      op = seekd(disknum);
      jbod_client_operation(op, mybuf);
      op = seekb(blocknum);
      jbod_client_operation(op, mybuf);

      // to read entries before the required write addresses.
      op = encode_op(JBOD_READ_BLOCK, 0, 0);
      jbod_client_operation(op, mybuf);
      int num_bytes_to_read_from_block = minimum(len, minimum(256, 256 - offset));
      num_read += num_bytes_to_read_from_block;
          if(cache_enabled()){

          cache_insert(disknum, blocknum, mybuf);
          }
    }
    // since block moves the pointer to the next block.
    op = seekd(disknum);
    jbod_client_operation(op, mybuf);
    op = seekb(blocknum);
    jbod_client_operation(op, mybuf);

    // initialising num_bytes_to_read_from_block
    int num_bytes_to_write_from_block = minimum(len, minimum(256, 256 - offset));
    // to avoid segmentation fault and memcpy errors
    if (i == 0)
    {
      memcpy(mybuf + offset, buf + num_write, num_bytes_to_write_from_block);
      i++;
    }
    else
    {
      memcpy(mybuf, buf + num_write, num_bytes_to_write_from_block);
    }

    op = encode_op(JBOD_WRITE_BLOCK, 0, 0);
    jbod_client_operation(op, mybuf);
    if (cache_enabled())
    {
      cache_update(disknum, blocknum, mybuf);
    }
    // updating memcpy and loop variables.
    num_write += num_bytes_to_write_from_block;
    len -= num_bytes_to_write_from_block;
    addr += num_bytes_to_write_from_block;
  }

  return num_write;
}
