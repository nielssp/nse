#ifndef MODULE_H
#define MODULE_H

typedef struct module Module;

typedef enum {
  TYPE_SCOPE,
  VALUE_SCOPE
} ScopeType;

typedef struct scope Scope;

struct scope {
  Module *module;
  const char *name;
  NseVal value;
  Scope *next;
  ScopeType type;
};

Scope *scope_push(Scope *scope, const char *name, NseVal value);
Scope *scope_pop(Scope *scope);
void scope_pop_until(Scope *start, Scope *end);
Scope *copy_scope(Scope *scope);
void delete_scope(Scope *scope);
NseVal scope_get(Scope *scope, const char *name);
NseVal scope_get_macro(Scope *scope, const char *name);

Module *create_module(const char *name);
void delete_module(Module *module);
Scope *use_module(Module *module);
Scope *use_module_types(Module *module);
void module_define(Module *module, const char *name, NseVal value);
void module_define_macro(Module *module, const char *name, NseVal value);
void module_define_type(Module *module, const char *name, NseVal value);

Symbol *find_symbol(const char *s);
Symbol *module_intern_symbol(Module *module, const char *s);
Symbol *intern_keyword(const char *s);
Symbol *intern_special(const char *s);

#endif
