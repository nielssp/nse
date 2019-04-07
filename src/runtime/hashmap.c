#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

typedef struct bucket Bucket;
struct bucket {
  HashMapEntry entry;
  Hash hash;
  char defined;
  char deleted;
};

struct hash_map {
  size_t size;
  size_t capacity;
  size_t mask;
  size_t upper_cap;
  size_t lower_cap;
  HashFunc hash_code_func;
  EqualityFunc equals_func;
  Bucket *buckets;
};

struct hash_map_iterator {
  HashMap *map;
  size_t next_bucket;
  Bucket *current;
};

HashMap *create_hash_map(HashFunc hash_code_func, EqualityFunc equals_func) {
  HashMap *map = malloc(sizeof(HashMap));
  if (!map) {
    return NULL;
  }
  map->size = 0;
  map->capacity = 8;
  map->mask = map->capacity - 1;
  map->upper_cap = map->capacity * 3 / 4;
  map->lower_cap = map->capacity / 4;
  map->buckets = calloc(map->capacity, sizeof(Bucket));
  if (!map->buckets) {
    free(map);
    return NULL;
  }
  map->hash_code_func = hash_code_func;
  map->equals_func = equals_func;
  return map;
}

void delete_hash_map(HashMap *map) {
  free(map->buckets);
  free(map);
}

size_t get_hash_map_size(HashMap *map) {
  return map->size;
}

HashMapIterator *create_hash_map_iterator(HashMap *map) {
  HashMapIterator *iterator = malloc(sizeof(HashMapIterator));
  if (!iterator) {
    return NULL;
  }
  iterator->map = map;
  iterator->next_bucket = 0;
  iterator->current = NULL;
  return iterator;
}

void delete_hash_map_iterator(HashMapIterator *iterator) {
  free(iterator);
}

HashMapEntry next_entry(HashMapIterator *iterator) {
  Bucket *current = NULL;
  while (iterator->next_bucket < iterator->map->capacity) {
    current = iterator->map->buckets + iterator->next_bucket;
    iterator->next_bucket++;
    if (current->defined) {
      return current->entry;
    }
  }
  return (HashMapEntry){.key = NULL, .value = NULL};
}

static int hash_map_resize(HashMap *map, size_t new_capacity) {
  size_t old_capacity = map->capacity;
  Bucket *old_buckets = map->buckets;
  map->capacity = new_capacity;
  map->buckets = calloc(map->capacity, sizeof(Bucket));
  if (!map->buckets) {
    map->buckets = old_buckets;
    map->capacity = old_capacity;
    return 0;
  }
  map->mask = new_capacity - 1;
  map->upper_cap = map->capacity * 3 / 4;
  map->lower_cap = map->capacity / 4;
  for (int i = 0; i < old_capacity; i++) {
    if (old_buckets[i].defined && !old_buckets[i].deleted) {
      Hash hash_code = old_buckets[i].hash;
      Hash hash = hash_code & map->mask;
      Bucket *bucket = map->buckets + hash;
      while (bucket->defined) {
        hash = (hash + 1) & map->mask;
        bucket = map->buckets + hash;
      }
      bucket->hash = hash_code;
      bucket->defined = 1;
      bucket->entry = old_buckets[i].entry;
    }
  }
  free(old_buckets);
  return 1;
}

int hash_map_add_generic(HashMap *map, void *key, void *value) {
  if (map->size > map->upper_cap) {
    if (!hash_map_resize(map, map->capacity << 1)) {
      return 0;
    }
  }
  Hash hash_code = map->hash_code_func(key);
  Hash hash = hash_code & map->mask;
  Bucket *bucket = map->buckets + hash;
  while (bucket->defined && !bucket->deleted) {
    if (map->equals_func(bucket->entry.key, key)) {
      return 0;
    }
    hash = (hash + 1) & map->mask;
    bucket = map->buckets + hash;
  }
  bucket->hash = hash_code;
  bucket->defined = 1;
  bucket->deleted = 0;
  bucket->entry.key = key;
  bucket->entry.value = value;
  map->size++;
  return 1;
}

HashMapEntry hash_map_remove_generic_entry(HashMap *map, const void *key) {
  Hash hash_code = map->hash_code_func(key);
  Hash hash = hash_code & map->mask;
  Bucket *bucket = map->buckets + hash;
  while (bucket->defined) {
    if (!bucket->deleted && hash_code == bucket->hash && map->equals_func(bucket->entry.key, key)) {
      HashMapEntry entry = bucket->entry;
      bucket->deleted = 1;
      map->size--;
      if (map->size < map->lower_cap && map->capacity > 8) {
        hash_map_resize(map, map->capacity >> 1);
      }
      return entry;
    }
    hash = (hash + 1) & map->mask;
    bucket = map->buckets + hash;
  }
  return (HashMapEntry){.key = NULL, .value = NULL};
}

HashMapEntry hash_map_lookup_generic_entry(HashMap *map, const void *key) {
  Hash hash_code = map->hash_code_func(key);
  Hash hash = hash_code & map->mask;
  Bucket *bucket = map->buckets + hash;
  while (bucket->defined) {
    if (!bucket->deleted && hash_code == bucket->hash && map->equals_func(bucket->entry.key, key)) {
      return bucket->entry;
    }
    hash = (hash + 1) & map->mask;
    bucket = map->buckets + hash;
  }
  return (HashMapEntry){.key = NULL, .value = NULL};
}

void *hash_map_remove_generic(HashMap *map, const void *key) {
  return hash_map_remove_generic_entry(map, key).value;
}

void *hash_map_lookup_generic(HashMap *map, const void *key) {
  return hash_map_lookup_generic_entry(map, key).value;
}

Hash string_hash(const char *key) {
  // jenkins
  Hash hash = 0;
  while (*key) {
    hash += *(key++);
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  hash += hash << 3;
  hash += hash << 11;
  hash ^= hash >> 15;
  return hash;
}

int string_equals(const char *a, const char *b) {
  return strcmp(a, b) == 0;
}

Hash pointer_hash(const void *p) {
  return HASH_ADD_PTR(p, INIT_HASH);
}

int pointer_equals(const void *a, const void *b) {
  return a == b;
}

DEFINE_HASH_MAP(dictionary, Dictionary, char *, char *, string_hash, string_equals)

DEFINE_HASH_SET(string_set, StringSet, char *, string_hash, string_equals)
