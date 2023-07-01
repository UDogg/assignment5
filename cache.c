#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) 
{
  if(cache!=NULL)
    return -1;
  if(num_entries<2||num_entries>4096)
    return -1;
  cache_size = num_entries;
  cache = calloc(cache_size, sizeof(cache_entry_t));
  return 1;

}

int cache_destroy(void)
{
  if(cache == NULL)
    return -1;
  free(cache);
  cache = NULL;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf)
{
  if(buf==NULL || cache ==NULL)
    return -1;
  num_queries++;
  clock++;

  for(int i = 0; i<cache_size; ++i)
  {
    if(cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num)
    {
      num_hits++;
      cache[i].access_time = clock;
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
      return 1;
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf)
{
  clock++;
  for(int i = 0; i < cache_size; ++i)
  {
    if (cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num)
    {
      cache[i].access_time = clock;
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
    }
    
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) 
{
  if(disk_num < 0 || disk_num > 16)
    return -1;
  if(block_num < 0 || block_num > 65536)
    return -1;
  if(buf == NULL || cache == NULL)
    return -1;

  clock++;
 

  for(int i = 0; i < cache_size; i++)
  {
    if(cache[i].valid && cache[i].disk_num == disk_num && cache[i].block_num == block_num)
    {
      return -1;
    }
    if(cache[i].valid == 0)
    {
      cache[i].valid = 1;
      cache[i].disk_num = disk_num;
      cache[i].block_num = block_num;
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
      cache[i].access_time = clock;
      return 1;
    }
  }
int lowest_time = 1e9;
int lowest_index = 0;

for(int i = 0; i < cache_size; i++)
{
  if(cache[i].access_time < lowest_time)
  {
    lowest_time = cache[i].access_time;
    lowest_index = i;
  }
}
cache[lowest_index].valid = 1;
cache[lowest_index].disk_num = disk_num;
cache[lowest_index].block_num = block_num;
memcpy(cache[lowest_index].block, buf, JBOD_BLOCK_SIZE);
cache[lowest_index].access_time = clock;
return 1;
}

bool cache_enabled(void) {
  if(cache == NULL)
    return false;
  return true;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}