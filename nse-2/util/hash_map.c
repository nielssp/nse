#include <stdlib.h>

#include "hash_map.h"
#include <string.h>

typedef struct bucket Bucket;
struct bucket {
  HashMapEntry entry;
  Bucket *next;
  Bucket *prev;
};

struct hash_map {
  size_t size;
  size_t capacity;
  Bucket **buckets;
};

struct hash_map_iterator {
  HashMap *map;
  size_t next_bucket;
  Bucket *current;
};

size_t primes[] = {
  13,
  23,
  47,
  97,
  198,
  389,
  769,
  1543,
  3079,
  6151,
  12289,
  24593,
  49157,
  98317,
  196613,
  393241,
  786433,
  1572869,
  3145739,
  6291469,
  12582917,
  25165843,
  50331653,
  100663319,
  201326611,
  402653189,
  805306457,
  1610612741
};

HashMap *create_hash_map() {
  HashMap *map = malloc(sizeof(HashMap));
  map->capacity = 0;
  map->size = 0;
  map->buckets = calloc(primes[map->capacity], sizeof(Bucket *));
  return map;
}

Bucket *create_bucket() {
  Bucket *bucket = malloc(sizeof(Bucket));
  bucket->entry.key = NULL;
  bucket->entry.value = NULL;
  bucket->next = NULL;
  bucket->prev = NULL;
  return bucket;
}

void delete_bucket(Bucket *bucket) {
  free(bucket);
}

void delete_hash_map(HashMap *map) {
  int i = 0;
  Bucket *bucket = NULL;
  Bucket *next = NULL;
  for (i = 0; i < primes[map->capacity]; i++) {
    bucket = map->buckets[i];
    while (bucket) {
      next = bucket->next;
      delete_bucket(bucket);
      bucket = next;
    }
  }
  free(map->buckets);
  free(map);
}

HashMapIterator *create_hash_map_iterator(HashMap *map) {
  HashMapIterator *iterator = malloc(sizeof(HashMapIterator));
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
  if (iterator->current) {
    current = iterator->current;
    iterator->current = current->next;
    return current->entry;
  }
  else {
    while (iterator->next_bucket < primes[iterator->map->capacity]) {
      current = iterator->map->buckets[iterator->next_bucket];
      iterator->next_bucket++;
      if (current) {
        iterator->current = current->next;
        return current->entry;
      }
    }
  }
  return (HashMapEntry){.key = NULL, .value = NULL};
}

void move_bucket(HashMap *map, Bucket *bucket, size_t hash_code_func(const void *)) {
  size_t hash_code = hash_code_func(bucket->entry.key);
  size_t hash = hash_code % primes[map->capacity];
  Bucket *existing = NULL;
  bucket->next = NULL;
  bucket->prev = NULL;
  if (map->buckets[hash]) {
    existing = map->buckets[hash];
    while (existing->next) {
      existing = existing->next;
    }
    existing->next = bucket;
    bucket->prev = existing;
  }
  else {
    map->buckets[hash] = bucket;
  }
}

void resize_hash_map(HashMap *map, size_t new_capacity, size_t hash_code_func(const void *)) {
  int i;
  Bucket **old_buckets = map->buckets;
  Bucket *bucket = NULL, *next = NULL;
  size_t old_capacity = map->capacity;
  map->buckets = calloc(primes[new_capacity], sizeof(Bucket *));
  map->capacity = new_capacity;
  for (i = 0; i < primes[old_capacity]; i++) {
    bucket = old_buckets[i];
    while (bucket) {
      next = bucket->next;
      move_bucket(map, bucket, hash_code_func);
      bucket = next;
    }
  }
  free(old_buckets);
}

void autoresize_hash_map(HashMap *map, size_t hash_code_func(const void *)) {
  size_t lower = map->capacity > 0 ? map->capacity - 1 : 0;
  size_t upper = map->capacity < 27 ? map->capacity + 1 : 27;
  if (map->size < primes[lower] && lower < map->capacity) {
    resize_hash_map(map, lower, hash_code_func);
  }
  else if (map->size > primes[upper] && upper > map->capacity) {
    resize_hash_map(map, upper, hash_code_func);
  }
}

int hash_map_add_generic(HashMap *map, const void *key, void *value,
    size_t hash_code_func(const void *), int equals_func(const void *, const void*)) {
  size_t hash_code = hash_code_func(key);
  size_t hash = hash_code % primes[map->capacity];
  Bucket *existing = NULL;
  Bucket *bucket = create_bucket();
  bucket->entry.key = key;
  bucket->entry.value = value;
  if (map->buckets[hash]) {
    existing = map->buckets[hash];
    do {
      if (hash_code == hash_code_func(existing->entry.key)) {
        if (equals_func(key, existing->entry.key)) {
          delete_bucket(bucket);
          return 0;
        }
      }
    } while (existing->next && (existing = existing->next));
    existing->next = bucket;
    bucket->prev = existing;
  }
  else {
    map->buckets[hash] = bucket;
  }
  map->size++;
  autoresize_hash_map(map, hash_code_func);
  return 1;
}

HashMapEntry hash_map_remove_generic_entry(HashMap *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void*)) {
  size_t hash_code = hash_code_func(key);
  size_t hash = hash_code % primes[map->capacity];
  Bucket *bucket = NULL;
  if (map->buckets[hash]) {
    bucket = map->buckets[hash];
    while (bucket) {
      if (hash_code == hash_code_func(bucket->entry.key)) {
        if (equals_func(key, bucket->entry.key)) {
          if (bucket->next) {
            bucket->next->prev = bucket->prev;
          }
          if (bucket->prev) {
            bucket->prev->next = bucket->next;
          }
          else if (bucket->next) {
            map->buckets[hash] = bucket->next;
          }
          else {
            map->buckets[hash] = NULL;
          }
          HashMapEntry entry = bucket->entry;
          delete_bucket(bucket);
          map->size--;
          autoresize_hash_map(map, hash_code_func);
          return entry;
        }
      }
      bucket = bucket->next;
    }
  }
  return (HashMapEntry){.key = NULL, .value = NULL};
}

void *hash_map_remove_generic(HashMap *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void*)) {
  return hash_map_remove_generic_entry(map, key, hash_code_func, equals_func).value;
}

HashMapEntry hash_map_lookup_generic_entry(HashMap *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void*)) {
  size_t hash_code = hash_code_func(key);
  size_t hash = hash_code % primes[map->capacity];
  Bucket *bucket = NULL;
  if (map->buckets[hash]) {
    bucket = map->buckets[hash];
    while (bucket) {
      if (hash_code == hash_code_func(bucket->entry.key)) {
        if (equals_func(key, bucket->entry.key)) {
          return bucket->entry;
        }
      }
      bucket = bucket->next;
    }
  }
  return (HashMapEntry){.key = NULL, .value = NULL};
}

void *hash_map_lookup_generic(HashMap *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void*)) {
  return hash_map_lookup_generic_entry(map, key, hash_code_func, equals_func).value;
}

size_t dictionary_hash(const void *p) {
  const char *s = (const char *)p;
  return (size_t)s[0];
}

int dictionary_equals(const void *a, const void *b) {
  const char *s1 = (const char *)a;
  const char *s2 = (const char *)b;
  return strcmp(s1, s2) == 0;
}

DEFINE_HASH_MAP(dictionary, Dictionary, char *, char *)
