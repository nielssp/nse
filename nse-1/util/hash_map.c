#include <stdlib.h>

#include "hash_map.h"
#include <string.h>

struct key_value {
  const void *key;
  void *value;
  key_value *next;
  key_value *prev;
};

struct hash_map {
  key_value **buckets;
  size_t size;
  size_t capacity;
};

struct hash_map_iterator {
  hash_map *map;
  size_t bucket;
  key_value *kv;
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

hash_map *hash_map_new() {
  hash_map *map = malloc(sizeof(hash_map));
  map->capacity = 0;
  map->size = 0;
  map->buckets = calloc(primes[map->capacity], sizeof(key_value *));
  return map;
}

key_value *key_value_new() {
  key_value *kv = malloc(sizeof(key_value));
  kv->key = NULL;
  kv->value = NULL;
  kv->next = NULL;
  kv->prev = NULL;
  return kv;
}

void key_value_delete(key_value *kv) {
  free(kv);
}

void hash_map_delete(hash_map *map) {
  int i = 0;
  key_value *kv = NULL;
  key_value *next = NULL;
  for (i = 0; i < primes[map->capacity]; i++) {
    if (map->buckets[i]) {
      kv = map->buckets[i];
      while (kv) {
        next = kv->next;
        key_value_delete(kv);
        kv = next;
      }
    }
  }
  free(map->buckets);
  free(map);
}

hash_map_iterator *hash_map_iterator_new(hash_map *map) {
  hash_map_iterator *iterator = malloc(sizeof(hash_map_iterator));
  iterator->map = map;
  iterator->bucket = 0;
  iterator->kv = NULL;
  return iterator;
}

void hash_map_iterator_delete(hash_map_iterator *iterator) {
  free(iterator);
}

key_value *next_key_value(hash_map_iterator *iterator) {
  key_value *kv = NULL;
  if (iterator->kv) {
    kv = iterator->kv;
    iterator->kv = kv->next;
    return kv;
  }
  else {
    if (iterator->bucket < primes[iterator->map->capacity]) {
      kv = iterator->map->buckets[iterator->bucket];
      iterator->bucket++;
      if (kv) {
        iterator->kv = kv->next;
        return kv;
      }
      else {
        while (iterator->bucket < primes[iterator->map->capacity]) {
          kv = iterator->map->buckets[iterator->bucket];
          iterator->bucket++;
          if (kv) {
            iterator->kv = kv->next;
            return kv;
          }
        }
      }
    }
  }
  return NULL;
}

void key_value_move(hash_map *map, key_value *kv, size_t hash_code_func(const void *)) {
  size_t hash_code = hash_code_func(kv->key);
  size_t hash = hash_code % primes[map->capacity];
  key_value *existing = NULL;
  kv->next = NULL;
  kv->prev = NULL;
  if (map->buckets[hash]) {
    existing = map->buckets[hash];
    while (existing->next) {
      existing = existing->next;
    }
    existing->next = kv;
    kv->prev = existing;
  }
  else {
    map->buckets[hash] = kv;
  }
}

void hash_map_resize(hash_map *map, size_t new_capacity, size_t hash_code_func(const void *)) {
  int i;
  key_value **old_buckets = map->buckets;
  key_value *kv = NULL, *next = NULL;
  size_t old_capacity = map->capacity;
  map->buckets = calloc(primes[new_capacity], sizeof(key_value *));
  map->capacity = new_capacity;
  for (i = 0; i < primes[old_capacity]; i++) {
    kv = old_buckets[i];
    while (kv) {
      next = kv->next;
      key_value_move(map, kv, hash_code_func);
      kv = next;
    }
  }
  free(old_buckets);
}

void hash_map_autoresize(hash_map *map, size_t hash_code_func(const void *)) {
  size_t lower = map->capacity > 0 ? map->capacity - 1 : 0;
  size_t upper = map->capacity < 27 ? map->capacity + 1 : 27;
  if (map->size < primes[lower] && lower < map->capacity) {
    hash_map_resize(map, lower, hash_code_func);
  }
  else if (map->size > primes[upper] && upper > map->capacity) {
    hash_map_resize(map, upper, hash_code_func);
  }
}

int hash_map_add_generic(hash_map *map, const void *key, void *value,
    size_t hash_code_func(const void *), int equals_func(const void *, const void*)) {
  size_t hash_code = hash_code_func(key);
  size_t hash = hash_code % primes[map->capacity];
  key_value *existing = NULL;
  key_value *kv = key_value_new();
  kv->key = key;
  kv->value = value;
  if (map->buckets[hash]) {
    existing = map->buckets[hash];
    do {
      if (hash_code == hash_code_func(existing->key)) {
        if (equals_func(key, existing->key)) {
          return 0;
        }
      }
    } while (existing->next && (existing = existing->next));
    existing->next = kv;
    kv->prev = existing;
  }
  else {
    map->buckets[hash] = kv;
  }
  map->size++;
  hash_map_autoresize(map, hash_code_func);
  return 1;
}

void *hash_map_remove_generic(hash_map *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void*)) {
  size_t hash_code = hash_code_func(key);
  size_t hash = hash_code % primes[map->capacity];
  key_value *kv = NULL;
  void *value = NULL;
  if (map->buckets[hash]) {
    kv = map->buckets[hash];
    while (kv) {
      if (hash_code == hash_code_func(kv->key)) {
        if (equals_func(key, kv->key)) {
          if (kv->next) {
            kv->next->prev = kv->prev;
          }
          if (kv->prev) {
            kv->prev->next = kv->next;
          }
          else if (kv->next) {
            map->buckets[hash] = kv->next;
          }
          else {
            map->buckets[hash] = NULL;
          }
          value = kv->value;
          key_value_delete(kv);
          map->size--;
          hash_map_autoresize(map, hash_code_func);
          return value;
        }
      }
      kv = kv->next;
    }
  }
  return NULL;
}

void *hash_map_lookup_generic(hash_map *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void*)) {
  size_t hash_code = hash_code_func(key);
  size_t hash = hash_code % primes[map->capacity];
  key_value *kv = NULL;
  if (map->buckets[hash]) {
    kv = map->buckets[hash];
    while (kv) {
      if (hash_code == hash_code_func(kv->key)) {
        if (equals_func(key, kv->key)) {
          return kv->value;
        }
      }
      kv = kv->next;
    }
  }
  return NULL;
}

key_value *hash_map_remove_generic_kv(hash_map *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void*)) {
  size_t hash_code = hash_code_func(key);
  size_t hash = hash_code % primes[map->capacity];
  key_value *kv = NULL;
  if (map->buckets[hash]) {
    kv = map->buckets[hash];
    while (kv) {
      if (hash_code == hash_code_func(kv->key)) {
        if (equals_func(key, kv->key)) {
          if (kv->next) {
            kv->next->prev = kv->prev;
          }
          if (kv->prev) {
            kv->prev->next = kv->next;
          }
          else if (kv->next) {
            map->buckets[hash] = kv->next;
          }
          else {
            map->buckets[hash] = NULL;
          }
          map->size--;
          hash_map_autoresize(map, hash_code_func);
          return kv;
        }
      }
      kv = kv->next;
    }
  }
  return NULL;
}

key_value *hash_map_lookup_generic_kv(hash_map *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void*)) {
  size_t hash_code = hash_code_func(key);
  size_t hash = hash_code % primes[map->capacity];
  key_value *kv = NULL;
  if (map->buckets[hash]) {
    kv = map->buckets[hash];
    while (kv) {
      if (hash_code == hash_code_func(kv->key)) {
        if (equals_func(key, kv->key)) {
          return kv;
        }
      }
      kv = kv->next;
    }
  }
  return NULL;
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

DEFINE_HASH_MAP(dictionary, char *, char *)
