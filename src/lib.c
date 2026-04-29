#include "lib.h"

#include "debug.h"
#include "process.h"
#include "stddef.h"

void spin_init(spinlock_t* lock) { lock->lock = 0; }

spinlock_t list_lock = {0};
void append_list_tail(struct HeadList* list, struct List* item) {
  spin_lock(&list_lock);
  item->next = NULL;

  if (is_list_empty(list)) {
    list->next = item;
    list->tail = item;
  } else {
    list->tail->next = item;
    list->tail = item;
  }
  spin_unlock(&list_lock);
}

struct List* remove_list_head(struct HeadList* list) {
  spin_lock(&list_lock);
  struct List* item;

  if (is_list_empty(list)) {
    spin_unlock(&list_lock);
    return NULL;
  }

  item = list->next;
  list->next = item->next;

  if (list->next == NULL) {
    spin_unlock(&list_lock);
    list->tail = NULL;
  }
  spin_unlock(&list_lock);
  return item;
}

bool is_list_empty(struct HeadList* list) { return (list->next == NULL); }

int get_list_count(struct HeadList* list) {
  int count = 0;
  struct List* current = list->next;

  while (current != NULL) {
    count++;
    current = current->next;
  }

  return count;
}

struct List* remove_list(struct HeadList* list, int wait) {
  struct List* current = list->next;
  struct List* prev = (struct List*)list;
  struct List* item = NULL;

  while (current != NULL) {
    if (((struct Process*)current)->wait == wait) {
      prev->next = current->next;
      item = current;

      if (list->next == NULL) {
        list->tail = NULL;
      } else if (current->next == NULL) {
        list->tail = prev;
      }

      break;
    }

    prev = current;
    current = current->next;
  }

  return item;
}
