#include <dlfcn.h>

#include "rtci.h"

NseVal c_load(NseVal args) {
  NseVal name = head(args);
  if (RESULT_OK(name)) {
    if (is_symbol(name)) {
      char *lib_name = to_symbol(name)->name;
      void *lib = dlopen(lib_name, RTLD_NOW);
      if (lib) {
        NseVal lib_ref = check_alloc(REFERENCE(create_reference(lib, (Destructor) dlclose)));
        if (RESULT_OK(lib_ref)) {
          return lib_ref;
        }
        dlclose(lib);
      } else {
        raise_error("cannot load library: %s", dlerror());
      }
    } else {
      raise_error("name of library must be a symbol");
    }
  }
  return undefined;
}

NseVal c_symbol(NseVal args) {
  NseVal name = head(args);
  if (RESULT_OK(name)) {
    if (is_symbol(name)) {
      NseVal ref = head(tail(args));
      if (RESULT_OK(ref)) {
        if (is_reference(ref)) {
          char *symbol_name = to_symbol(name)->name;
          void *lib = to_reference(ref);
          void *value = dlsym(lib, symbol_name);
          if (value) {
            NseVal value_ref = check_alloc(REFERENCE(create_reference(value, NULL)));
            if (RESULT_OK(value_ref)) {
              return value_ref;
            }
          } else {
            raise_error("cannot find symbol: %s", dlerror());
          }
        } else {
          raise_error("library must be a reference");
        }
      }
    } else {
      raise_error("name of library must be a symbol");
    }
  }
  return undefined;
}
