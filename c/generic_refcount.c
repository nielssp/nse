#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#define copy(obj) (copy_internal((obj), offsetof(typeof(*obj), refs)))
#define delete(obj) (delete_internal((obj), offsetof(typeof(*obj), refs), free))

typedef struct cat {
  size_t refs;
  int meow;
} Cat;

typedef struct dog {
  size_t refs;
  int bark;
} Dog;

Dog *create_dog() {
  Dog *dog = malloc(sizeof(Dog));
  dog->refs = 1;
  printf("%p: create dog\n", dog);
  return dog;
}

Cat *create_cat() {
  Cat *cat = malloc(sizeof(Cat));
  cat->refs = 1;
  printf("%p: create cat\n", cat);
  return cat;
}

void *copy_internal(void *object, size_t count_offset) {
  size_t *counter = (size_t *)(object + count_offset);
  (*counter)++;
  printf("%p: ref++ (%zd)\n", object, *counter);
  return object;
}

void delete_internal(void *object, size_t count_offset, void destructor(void *)) {
  size_t *counter = (size_t *)(object + count_offset);
  if (*counter > 0) {
    (*counter)--;
  }
  printf("%p: ref-- (%zd)\n", object, *counter);
  if (*counter == 0) {
    destructor(object);
  }
}

int main() {
  Dog *dog = create_dog();
  copy(dog);
  delete(dog);
  delete(dog);
  return 0;
}
