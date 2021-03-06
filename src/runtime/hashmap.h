#ifndef HASHMAP_H
#define HASHMAP_H

#include <stdint.h>

#define NULL_HASH_MAP {NULL}
#define HASH_MAP_INITIALIZED(name) ((name).map != NULL)
#define NULL_HASH_SET {NULL}
#define HASH_SET_INITIALIZED(name) ((name).map != NULL)

#define DECLARE_HASH_MAP(name, type_name, key_type, value_type)\
  typedef struct {\
    HashMap *map;\
  } type_name;\
  typedef struct {\
    key_type key;\
    value_type value;\
  } type_name ## Entry;\
  typedef struct {\
    HashMapIterator *iterator;\
  } type_name ## Iterator;\
  type_name create_ ## name();\
  void delete_ ## name(type_name map);\
  int name ## _add(type_name map, key_type key, value_type value);\
  value_type name ## _remove(type_name map, const key_type key);\
  value_type name ## _lookup(type_name map, const key_type key);\
  type_name ## Entry name ## _remove_entry(type_name map, const key_type key);\
  type_name ## Entry name ## _lookup_entry(type_name map, const key_type key);\
  type_name ## Iterator create_ ## name ## _iterator(type_name map);\
  void delete_ ## name ## _iterator(type_name ## Iterator iterator);\
  type_name ## Entry name ## _next(type_name ## Iterator iterator);\
  key_type name ## _next_key(type_name ## Iterator iterator);\
  value_type name ## _next_value(type_name ## Iterator iterator);


#define DECLARE_HASH_SET(name, type_name, value_type)\
  typedef struct name type_name;\
  typedef struct name ## _iterator type_name ## Iterator;\
  type_name create_ ## name();\
  void delete_ ## name(type_name set);\
  int name ## _add(type_name set, value_type value);\
  const value_type name ## _remove(type_name set, const value_type value);\
  int name ## _contains(type_name set, const value_type value);\
  type_name ## Iterator create_ ## name ## _iterator(type_name set);\
  void delete_ ## name ## _iterator(type_name ## Iterator iterator);\
  value_type name ## _next(type_name ## Iterator iterator);

#define DEFINE_HASH_MAP(name, type_name, key_type, value_type, hash_func, equals_func)\
  type_name create_ ## name() {\
    return (type_name){.map = create_hash_map((HashFunc) hash_func, (EqualityFunc) equals_func)};\
  }\
  void delete_ ## name(type_name map) {\
    delete_hash_map(map.map);\
  }\
  int name ## _add(type_name map, key_type key, value_type value) {\
    return hash_map_add_generic(map.map, (void *)key, (void *)value);\
  }\
  value_type name ## _remove(type_name map, const key_type key) {\
    return (value_type)hash_map_remove_generic(map.map, (void *)key);\
  }\
  value_type name ## _lookup(type_name map, const key_type key) {\
    return (value_type)hash_map_lookup_generic(map.map, (void *)key);\
  }\
  type_name ## Entry name ## _remove_entry(type_name map, const key_type key) {\
    HashMapEntry entry = hash_map_remove_generic_entry(map.map, (void *)key);\
    return (type_name ## Entry){.key = (key_type)entry.key, .value = (value_type)entry.value};\
  }\
  type_name ## Entry name ## _lookup_entry(type_name map, const key_type key) {\
    HashMapEntry entry = hash_map_lookup_generic_entry(map.map, (void *)key);\
    return (type_name ## Entry){.key = (key_type)entry.key, .value = (value_type)entry.value};\
  }\
  type_name ## Iterator create_ ## name ## _iterator(type_name map) {\
    return (type_name ## Iterator){.iterator = create_hash_map_iterator(map.map)};\
  }\
  void delete_ ## name ## _iterator(type_name ## Iterator iterator) {\
    delete_hash_map_iterator(iterator.iterator);\
  }\
  type_name ## Entry name ## _next(type_name ## Iterator iterator) {\
    HashMapEntry entry  = next_entry(iterator.iterator);\
    return (type_name ## Entry){.key = (key_type)entry.key, .value = (value_type)entry.value};\
  }\
  key_type name ## _next_key(type_name ## Iterator iterator) {\
    return (key_type)next_entry(iterator.iterator).key;\
  }\
  value_type name ## _next_value(type_name ## Iterator iterator) {\
    return (value_type)next_entry(iterator.iterator).value;\
  }

#define DEFINE_HASH_SET(name, type_name, value_type, hash_func, equals_func)\
  struct name {\
    HashMap *map;\
  };\
  struct name ## _iterator {\
    HashMapIterator *iterator;\
  };\
  type_name create_ ## name() {\
    return (type_name){.map = create_hash_map((HashFunc) hash_func, (EqualityFunc) equals_func)};\
  }\
  void delete_ ## name(type_name set) {\
    delete_hash_map(set.map);\
  }\
  int name ## _add(type_name set, value_type value) {\
    return hash_map_add_generic(set.map, value, NULL);\
  }\
  const value_type name ## _remove(type_name set, const value_type value) {\
    return (value_type)hash_map_remove_generic_entry(set.map, value).key;\
  }\
  int name ## _contains(type_name set, const value_type value) {\
    return hash_map_lookup_generic_entry(set.map, value).key != NULL;\
  }\
  type_name ## Iterator create_ ## name ## _iterator(type_name set) {\
    return (type_name ## Iterator){.iterator = create_hash_map_iterator(set.map)};\
  }\
  void delete_ ## name ## _iterator(type_name ## Iterator iterator) {\
    delete_hash_map_iterator(iterator.iterator);\
  }\
  value_type name ## _next(type_name ## Iterator iterator) {\
    return (value_type)next_entry(iterator.iterator).key;\
  }

#define DEFINE_PRIVATE_HASH_MAP(name, type_name, key_type, value_type, hash_func, equals_func)\
  typedef struct {\
    HashMap *map;\
  } type_name;\
  typedef struct {\
    key_type key;\
    value_type value;\
  } type_name ## Entry;\
  typedef struct {\
    HashMapIterator *iterator;\
  } type_name ## Iterator;\
  DEFINE_HASH_MAP(name, type_name, key_type, value_type, hash_func, equals_func)

#define DEFINE_PRIVATE_HASH_SET(name, type_name, key_type, hash_func, equals_func)\
  typedef struct name type_name;\
  typedef struct name ## _iterator type_name ## Iterator;\
  DEFINE_HASH_SET(name, type_name, value_type, hash_func, equals_func)

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

typedef struct hash_map HashMap;
typedef struct hash_map_iterator HashMapIterator;
typedef struct {
  void *key;
  void *value;
} HashMapEntry;

HashMap *create_hash_map(HashFunc hash_code_func, EqualityFunc equals_func);
void delete_hash_map(HashMap *map);
size_t get_hash_map_size(HashMap *map);

HashMapIterator *create_hash_map_iterator(HashMap *map);
void delete_hash_map_iterator(HashMapIterator *iterator);

HashMapEntry next_entry(HashMapIterator *iterator);

int hash_map_add_generic(HashMap *map, void *key, void *value);
void *hash_map_remove_generic(HashMap *map, const void *key);
void *hash_map_lookup_generic(HashMap *map, const void *key);

HashMapEntry hash_map_remove_generic_entry(HashMap *map, const void *key);
HashMapEntry hash_map_lookup_generic_entry(HashMap *map, const void *key);

Hash string_hash(const char *key);
int string_equals(const char *a, const char *b);
Hash pointer_hash(const void *p);
int pointer_equals(const void *a, const void *b);

DECLARE_HASH_MAP(dictionary, Dictionary, char *, char *)
DECLARE_HASH_SET(string_set, StringSet, char *)

#endif
