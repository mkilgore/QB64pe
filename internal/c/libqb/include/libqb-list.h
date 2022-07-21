#ifndef INCLUDE_LIBQB_LIST_H
#define INCLUDE_LIBQB_LIST_H

#include <stdint.h>

// The actual definition is in libqb.cpp, this is just the bare minimum needed
// by external users.
struct list;

intptr_t list_add(list *);
void *list_get(list *, intptr_t index);

#endif
