#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdlib.h>

#define DECLARE_HASH_MAP(name, key_type, value_type)\
  int name ## _add(hash_map *map, const key_type key, value_type value);\
  value_type name ## _remove(hash_map *map, const key_type key);\
  value_type name ## _lookup(hash_map *map, const key_type key);\
  key_value *name ## _remove_kv(hash_map *map, const key_type key);\
  key_value *name ## _lookup_kv(hash_map *map, const key_type key);

#define DEFINE_HASH_MAP(name, key_type, value_type)\
  int name ## _add(hash_map *map, const key_type key, value_type value) {\
    return hash_map_add_generic(map, key, value, name ## _hash, name ## _equals);\
  }\
  value_type name ## _remove(hash_map *map, const key_type key){\
    return (value_type)hash_map_remove_generic(map, key, name ## _hash, name ## _equals);\
  }\
  value_type name ## _lookup(hash_map *map, const key_type key){\
    return (value_type)hash_map_lookup_generic(map, key, name ## _hash, name ## _equals);\
  }\
  key_value *name ## _remove_kv(hash_map *map, const key_type key){\
    return hash_map_remove_generic_kv(map, key, name ## _hash, name ## _equals);\
  }\
  key_value *name ## _lookup_kv(hash_map *map, const key_type key){\
    return hash_map_lookup_generic_kv(map, key, name ## _hash, name ## _equals);\
  }

typedef struct hash_map hash_map;
typedef struct hash_map_iterator hash_map_iterator;
typedef struct key_value key_value;

hash_map *hash_map_new();
void hash_map_delete(hash_map *map);

hash_map_iterator *hash_map_iterator_new(hash_map *map);
void hash_map_iterator_delete(hash_map_iterator *iterator);

key_value *next_key_value(hash_map_iterator *iterator);

void key_value_delete(key_value *kv);

int hash_map_add_generic(hash_map *map, const void *key, void *value,
    size_t hash_code_func(const void *), int equals_func(const void *, const void *));
void *hash_map_remove_generic(hash_map *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void *));
void *hash_map_lookup_generic(hash_map *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void *));

key_value *hash_map_remove_generic_kv(hash_map *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void *));
key_value *hash_map_lookup_generic_kv(hash_map *map, const void *key, 
    size_t hash_code_func(const void *), int equals_func(const void *, const void *));

DECLARE_HASH_MAP(dictionary, char *, char *)

#endif
