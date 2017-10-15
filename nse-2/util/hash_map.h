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

#define DEFINE_HASH_MAP(name, type_name, key_type, value_type)\
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
    return hash_map_add_generic(map.map, key, value, name ## _hash, name ## _equals);\
  }\
  value_type name ## _remove(type_name map, const key_type key) {\
    return (value_type)hash_map_remove_generic(map.map, key, name ## _hash, name ## _equals);\
  }\
  value_type name ## _lookup(type_name map, const key_type key) {\
    return (value_type)hash_map_lookup_generic(map.map, key, name ## _hash, name ## _equals);\
  }\
  HashMapEntry name ## _remove_entry(type_name map, const key_type key) {\
    return hash_map_remove_generic_entry(map.map, key, name ## _hash, name ## _equals);\
  }\
  HashMapEntry name ## _lookup_entry(type_name map, const key_type key) {\
    return hash_map_lookup_generic_entry(map.map, key, name ## _hash, name ## _equals);\
  }\
  HashMapIterator *create_ ## name ## _iterator(type_name map) {\
    return create_hash_map_iterator(map.map);\
  }

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

DECLARE_HASH_MAP(dictionary, Dictionary, char *, char *)

#endif
