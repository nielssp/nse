#include "type."

int is_subtype_of(Type *a, Type *b) {
  switch (b->type) {
    case BASE_TYPE_ANY:
      return 1;
    case BASE_TYPE_NIL:
      return a->type == BASE_TYPE_NIL;
    case BASE_TYPE_I64:
      if (a->type == BASE_TYPE_I64 || a->type == BASE_TYPE_U32) {
        return 1;
      }
    case BASE_TYPE_I32:
      if (a->type == BASE_TYPE_I32 || a->type == BASE_TYPE_U16) {
        return 1;
      }
    case BASE_TYPE_I16:
      if (a->type == BASE_TYPE_I16 || a->type == BASE_TYPE_U8) {
        return 1;
      }
    case BASE_TYPE_I8:
      return a->type == BASE_TYPE_I8;
    case BASE_TYPE_U64:
      if (a->type == BASE_TYPE_U64) {
        return 1;
      }
    case BASE_TYPE_U32:
      if (a->type == BASE_TYPE_U32) {
        return 1;
      }
    case BASE_TYPE_U16:
      if (a->type == BASE_TYPE_U16) {
        return 1;
      }
    case BASE_TYPE_U8:
      return a->type == BASE_TYPE_U8;
    case BASE_TYPE_F64:
      if (a->type == BASE_TYPE_F64) {
        return 1;
      }
    case BASE_TYPE_F32:
      return a->type == BASE_TYPE_F32;
    case BASE_TYPE_STRING:
      return a->type == BASE_TYPE_STRING;
    case BASE_TYPE_ANY_SYMBOL:
      return a->type == BASE_TYPE_SYMBOL || a->type == BASE_TYPE_ANY_SYMBOL;
    case BASE_TYPE_TYPE:
      return a->type == BASE_TYPE_TYPE;
    case BASE_TYPE_SYMBOL:
      return a->type == BASE_TYPE_SYMBOL && strcmp(a->var_name, b->var_name) == 0;
    case BASE_TYPE_TYPE_VAR:
      return 0; // todo: find out if this makes sense
    case BASE_TYPE_QUOTE:
      return a->type == BASE_TYPE_QUOTE && is_subtype_of(a->param_a, b->param_a);
    case BASE_TYPE_TYPE_QUOTE:
      return a->type == BASE_TYPE_TYPE_QUOTE && is_subtype_of(a->param_a, b->param_a);
    case BASE_TYPE_SYNTAX:
      return a->type == BASE_TYPE_SYNTAX && is_subtype_of(a->param_a, b->param_a);
    case BASE_TYPE_CONS:
      return a->type == BASE_TYPE_CONS && is_subtype_of(a->param_a, b->param_a) && is_subtype_of(a->param_b, b->param_b);
    case BASE_TYPE_FUNC:
      return a->type == BASE_TYPE_FUNC && is_subtype_of(a->param_a, b->param_a) && is_subtype_of(a->param_b, b->param_b);
    case BASE_TYPE_UNION:
      return is_subtype_of(a, b->param_a) || is_subtype_of(a, b->param_b);
    case BASE_TYPE_RECUR:
      while (1) {
      }
      return 1;
  }
}
