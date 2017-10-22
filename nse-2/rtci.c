#include <dlfcn.h>

#include "rtci.h"

NseVal c_load(NseVal args) {
  NseVal name = head(args);
  if (RESULT_OK(name)) {
    if (is_symbol(name)) {
      char *lib_name = to_symbol(name);
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

