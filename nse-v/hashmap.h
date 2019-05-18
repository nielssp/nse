#ifndef NSE_HASHMAP_H
#define NSE_HASHMAP_H

#include <stddef.h>
#include <stdint.h>

#define GET_BYTE(N, OBJ) (((uint8_t *)&(OBJ))[N])
#define HASH_ADD_BYTE(BYTE, HASH) (((HASH) * FNV_PRIME) ^ (BYTE))

#if UINT64_MAX == UINTPTR_MAX
#define HASH_SIZE_64
typedef uint64_t Hash;
#define INIT_HASH 0xcbf29ce484222325ul
#define FNV_PRIME 1099511628211ul
#define HASH_ADD_PTR(PTR, HASH) \
  HASH_ADD_BYTE(GET_BYTE(0, PTR), \
      HASH_ADD_BYTE(GET_BYTE(1, PTR), \
        HASH_ADD_BYTE(GET_BYTE(2, PTR), \
          HASH_ADD_BYTE(GET_BYTE(3, PTR), \
            HASH_ADD_BYTE(GET_BYTE(4, PTR), \
              HASH_ADD_BYTE(GET_BYTE(5, PTR), \
                HASH_ADD_BYTE(GET_BYTE(6, PTR), \
                  HASH_ADD_BYTE(GET_BYTE(7, PTR), HASH))))))))
#else
#define HASH_SIZE_32
typedef uint32_t Hash;
#define INIT_HASH 0x811c9dc5
#define FNV_PRIME 16777619
#define HASH_ADD_PTR(PTR, HASH) \
  HASH_ADD_BYTE(GET_BYTE(0, PTR), \
      HASH_ADD_BYTE(GET_BYTE(1, PTR), \
        HASH_ADD_BYTE(GET_BYTE(2, PTR), \
          HASH_ADD_BYTE(GET_BYTE(3, PTR), HASH))))
#endif

typedef Hash (* HashFunc)(const void *);
typedef int (* EqualityFunc)(const void *, const void *);

#define NULL_HASH_MAP {.header.buckets = NULL}
#define HASH_MAP_INITIALIZED(name) ((name).header.buckets != NULL)

#define DECLARE_HASH_MAP(NAME, TYPE_NAME, KEY_TYPE, VALUE_TYPE) \
  typedef struct {\
    GenericHashMap header;\
  } TYPE_NAME;\
  typedef struct {\
    KEY_TYPE key;\
    VALUE_TYPE value;\
  } TYPE_NAME ## Entry;\
  int init_ ## NAME(TYPE_NAME *map);\
  void delete_ ## NAME(TYPE_NAME *map);\
  Hash NAME ## _hash(const TYPE_NAME ## Entry *entry);\
  int NAME ## _equals(const TYPE_NAME ## Entry *a, const TYPE_NAME ## Entry *b);\
  int NAME ## _add(TYPE_NAME *map, KEY_TYPE key, VALUE_TYPE value);\
  int NAME ## _remove_entry(TYPE_NAME *map, KEY_TYPE key, TYPE_NAME ## Entry *entry);\
  int NAME ## _remove(TYPE_NAME *map, KEY_TYPE key, VALUE_TYPE *value);\
  int NAME ## _get_entry(TYPE_NAME *map, KEY_TYPE key, TYPE_NAME ## Entry *entry);\
  int NAME ## _get(TYPE_NAME *map, KEY_TYPE key, VALUE_TYPE *value);\
  HashMapIterator NAME ## _iterate(TYPE_NAME *map);\
  int NAME ## _next_entry(HashMapIterator *iterator, TYPE_NAME ## Entry *entry);\
  int NAME ## _next_key(HashMapIterator *iterator, KEY_TYPE *key);\
  int NAME ## _next(HashMapIterator *iterator, VALUE_TYPE *value);


#define DEFINE_HASH_MAP(NAME, TYPE_NAME, KEY_TYPE, VALUE_TYPE, KEY_HASH_FUNC, KEY_EQUALS_FUNC) \
  int init_ ## NAME(TYPE_NAME *map) {\
    return init_generic_hash_map(&map->header, sizeof(TYPE_NAME ## Entry), (HashFunc) NAME ## _hash, (EqualityFunc) NAME ## _equals);\
  }\
  void delete_ ## NAME(TYPE_NAME *map) {\
    delete_generic_hash_map(&map->header);\
  }\
  Hash NAME ## _hash(const TYPE_NAME ## Entry *entry) {\
    return KEY_HASH_FUNC(entry->key);\
  }\
  int NAME ## _equals(const TYPE_NAME ## Entry *a, const TYPE_NAME ## Entry *b) {\
    return KEY_EQUALS_FUNC(a->key, b->key);\
  }\
  int NAME ## _add(TYPE_NAME *map, KEY_TYPE key, VALUE_TYPE value) {\
    return generic_hash_map_add(&map->header, &(TYPE_NAME ## Entry){ .key = key, .value = value });\
  }\
  int NAME ## _remove_entry(TYPE_NAME *map, KEY_TYPE key, TYPE_NAME ## Entry *entry) {\
    return generic_hash_map_remove(&map->header, &(TYPE_NAME ## Entry){ .key = key }, entry);\
  }\
  int NAME ## _remove(TYPE_NAME *map, KEY_TYPE key, VALUE_TYPE *value) {\
    TYPE_NAME ## Entry entry;\
    if (generic_hash_map_remove(&map->header, &(TYPE_NAME ## Entry){ .key = key }, &entry)) {\
      if (value) *value = entry.value;\
      return 1;\
    };\
    return 0;\
  }\
  int NAME ## _get_entry(TYPE_NAME *map, KEY_TYPE key, TYPE_NAME ## Entry *entry) {\
    return generic_hash_map_get(&map->header, &(TYPE_NAME ## Entry){ .key = key }, entry);\
  }\
  int NAME ## _get(TYPE_NAME *map, KEY_TYPE key, VALUE_TYPE *value) {\
    TYPE_NAME ## Entry entry;\
    if (generic_hash_map_get(&map->header, &(TYPE_NAME ## Entry){ .key = key }, &entry)) {\
      if (value) *value = entry.value;\
      return 1;\
    };\
    return 0;\
  }\
  HashMapIterator NAME ## _iterate(TYPE_NAME *map) {\
    return generic_hash_map_iterate(&map->header);\
  }\
  int NAME ## _next_entry(HashMapIterator *iterator, TYPE_NAME ## Entry *entry) {\
    return generic_hash_map_next(iterator, entry);\
  }\
  int NAME ## _next_key(HashMapIterator *iterator, KEY_TYPE *key) {\
    TYPE_NAME ## Entry entry;\
    if (generic_hash_map_next(iterator, &entry)) {\
      if (key) *key = entry.key;\
      return 1;\
    };\
    return 0;\
  }\
  int NAME ## _next(HashMapIterator *iterator, VALUE_TYPE *value) {\
    TYPE_NAME ## Entry entry;\
    if (generic_hash_map_next(iterator, &entry)) {\
      if (value) *value = entry.value;\
      return 1;\
    };\
    return 0;\
  }


typedef struct {
  Hash hash;
  char defined;
  char deleted;
} GenericBucket;

typedef struct {
  size_t size;
  size_t capacity;
  size_t mask;
  size_t upper_cap;
  size_t lower_cap;
  size_t entry_size;
  size_t bucket_size;
  size_t bucket_size_shift;
  HashFunc hash_code_func;
  EqualityFunc equals_func;
  uint8_t *buckets;
} GenericHashMap;

typedef struct {
  GenericHashMap *map;
  size_t next_bucket;
} HashMapIterator;


int init_generic_hash_map(GenericHashMap *map, size_t entry_size, HashFunc hash_code_func, EqualityFunc equals_func);

void delete_generic_hash_map(GenericHashMap *map);

size_t get_hash_map_size(void *map);

HashMapIterator generic_hash_map_iterate(GenericHashMap *map);

int generic_hash_map_next(HashMapIterator *iterator, void *result);

int generic_hash_map_resize(GenericHashMap *map, size_t new_capacity);

int generic_hash_map_add(GenericHashMap *map, const void *entry);

int generic_hash_map_remove(GenericHashMap *map, const void *entry, void *removed);

int generic_hash_map_get(GenericHashMap *map, const void *entry, void *result);


Hash string_hash(const char *key);
int string_equals(const char *a, const char *b);
Hash pointer_hash(const void *p);
int pointer_equals(const void *a, const void *b);

DECLARE_HASH_MAP(dictionary, Dictionary, char *, char *)

#endif
