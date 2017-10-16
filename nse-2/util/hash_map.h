#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdlib.h>

#define DECLARE_HASH_MAP(name, type_name, key_type, value_type)\
  typedef struct name type_name;\
  type_name create ## name();\
  void delete ## name(type_name map);\
  int name ## _add(type_name map, const key_type key, value_type value);\
  value_type name ## _remove(type_name map, const key_type key);\
  value_type name ## _lookup(type_name map, const key_type key);\
  HashMapEntry name ## _remove_entry(type_name map, const key_type key);\
  HashMapEntry name ## _lookup_entry(type_name map, const key_type key);\
  HashMapIterator *create_ ## name ## _iterator(type_name map);

#define DECLARE_HASH_SET(name, type_name, value_type)\
  typedef struct name type_name;\
  typedef struct name ## _iterator type_name ## Iterator;\
  type_name create ## name();\
  void delete ## name(type_name set);\
  int name ## _add(type_name set, const value_type value);\
  const value_type name ## _remove(type_name set, const value_type value);\
  int name ## _contains(type_name set, const value_type value);\
  type_name ## Iterator create_ ## name ## _iterator(type_name set);\
  void delete_ ## name ## _iterator(type_name ## Iterator iterator);\
  const value_type name ## _next(type_name ## Iterator iterator);

#define DEFINE_HASH_MAP(name, type_name, key_type, value_type, hash_func, equals_func)\
  struct name {\
    HashMap *map;\
  };\
  type_name create_ ## name() {\
    return (type_name){.map = create_hash_map()};\
  }\
  void delete_ ## name(type_name map) {\
    delete_hash_map(map.map);\
  }\
  int name ## _add(type_name map, const key_type key, value_type value) {\
    return hash_map_add_generic(map.map, key, value, hash_func, equals_func);\
  }\
  value_type name ## _remove(type_name map, const key_type key) {\
    return (value_type)hash_map_remove_generic(map.map, key, hash_func, equals_func);\
  }\
  value_type name ## _lookup(type_name map, const key_type key) {\
    return (value_type)hash_map_lookup_generic(map.map, key, hash_func, equals_func);\
  }\
  HashMapEntry name ## _remove_entry(type_name map, const key_type key) {\
    return hash_map_remove_generic_entry(map.map, key, hash_func, equals_func);\
  }\
  HashMapEntry name ## _lookup_entry(type_name map, const key_type key) {\
    return hash_map_lookup_generic_entry(map.map, key, hash_func, equals_func);\
  }\
  HashMapIterator *create_ ## name ## _iterator(type_name map) {\
    return create_hash_map_iterator(map.map);\
  }

#define DEFINE_HASH_SET(name, type_name, value_type, hash_func, equals_func)\
  struct name {\
    HashMap *map;\
  };\
  struct name ## _iterator {\
    HashMapIterator *iterator;\
  };\
  type_name create_ ## name() {\
    return (type_name){.map = create_hash_map()};\
  }\
  void delete_ ## name(type_name set) {\
    delete_hash_map(set.map);\
  }\
  int name ## _add(type_name set, const value_type value) {\
    return hash_map_add_generic(set.map, value, NULL, hash_func, equals_func);\
  }\
  const value_type name ## _remove(type_name set, const value_type value) {\
    return (value_type)hash_map_remove_generic_entry(set.map, value, hash_func, equals_func).key;\
  }\
  int name ## _contains(type_name set, const value_type value) {\
    return hash_map_lookup_generic_entry(set.map, value, hash_func, equals_func).key != NULL;\
  }\
  type_name ## Iterator create_ ## name ## _iterator(type_name set) {\
    return (type_name ## Iterator){.iterator = create_hash_map_iterator(set.map)};\
  }\
  void delete_ ## name ## _iterator(type_name ## Iterator iterator) {\
    delete_hash_map_iterator(iterator.iterator);\
  }\
  const value_type name ## _next(type_name ## Iterator iterator) {\
    return (value_type)next_entry(iterator.iterator).key;\
  }

#define DEFINE_PRIVATE_HASH_MAP(name, type_name, key_type, value_type, hash_func, equals_func)\
  typedef struct name type_name;\
  typedef struct name ## _iterator type_name ## Iterator;\
  DEFINE_HASH_MAP(name, type_name, key_type, value_type, hash_func, equals_func)

#define DEFINE_PRIVATE_HASH_SET(name, type_name, key_type, hash_func, equals_func)\
  typedef struct name type_name;\
  typedef struct name ## _iterator type_name ## Iterator;\
  DEFINE_HASH_SET(name, type_name, value_type, hash_func, equals_func)

typedef struct hash_map HashMap;
typedef struct hash_map_iterator HashMapIterator;
typedef struct {
  const void *key;
  void *value;
} HashMapEntry;

HashMap *create_hash_map();
void delete_hash_map(HashMap *map);

HashMapIterator *create_hash_map_iterator(HashMap *map);
void delete_hash_map_iterator(HashMapIterator *iterator);

HashMapEntry next_entry(HashMapIterator *iterator);

int hash_map_add_generic(HashMap *map, const void *key, void *value,
    size_t hash_code_func(const void *), int equals_func(const void *, const void *));
void *hash_map_remove_generic(HashMap *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void *));
void *hash_map_lookup_generic(HashMap *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void *));

HashMapEntry hash_map_remove_generic_entry(HashMap *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void *));
HashMapEntry hash_map_lookup_generic_entry(HashMap *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void *));

size_t string_hash(const void *p);
int string_equals(const void *a, const void *b);

DECLARE_HASH_MAP(dictionary, Dictionary, char *, char *)
DECLARE_HASH_SET(string_set, StringSet, char *)

#endif
