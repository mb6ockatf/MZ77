#include <config.h>
#include <stdlib.h>
#include <string.h>
#include "dt.h"

void
HashTable_init (LZHashTable *ht)
{
  for (uint32_t i = 0; i < HT_SIZE; i++)
    ht->buckets[i] = NULL;
}

void
HashTable_insert (LZHashTable *ht, uint32_t hash, DWORD pos)
{
  HashNode *node = malloc (sizeof (HashNode));
  if (!node)
    return;
  node->position = pos;
  uint32_t idx = hash % HT_SIZE;
  node->next = ht->buckets[idx];
  ht->buckets[idx] = node;
}

void
HashTable_clear (LZHashTable *ht)
{
  for (uint32_t i = 0; i < HT_SIZE; i++)
    {
      HashNode *cur = ht->buckets[i];
      while (cur)
	{
	  HashNode *next = cur->next;
	  free (cur);
	  cur = next;
	}
      ht->buckets[i] = NULL;
    }
}
