#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "search.h"
#include "loew.h"

extern char *loewlib_search(Thread *t);

void main() {
  char **results;
  unsigned int count;

  results = search_run("mono", "cheesecake and rice krispies and quirka");
  count = 0;
  while(results[count]) {
    printf("Results:%s\n", results[count]);
    free(results[count]);
    count++;
  }
  free(results);

  results = search_run("mono", "cheese");
  count = 0;
  while(results[count]) {
    printf("Results:%s\n", results[count]);
    free(results[count]);
    count++;
  }
  free(results);
}

#ifdef FAKE
void main() {
  Thread *t = loew_initthread(NULL, NULL);
  
  t->stack[t->sp] = gmalloc(sizeof(Data));
  t->stack[t->sp]->type = DATA_STR;
  strncpy(t->stack[t->sp]->string.data, "cheese", MAXSTR - 1);
  t->sp++;

  t->stack[t->sp] = gmalloc(sizeof(Data));
  t->stack[t->sp]->type = DATA_STR;
  strncpy(t->stack[t->sp]->string.data, "mono", MAXSTR - 1);
  t->sp++;

  loewlib_search(t);
printf("Stack ptr: %d\n", t->sp);

  t->stack[t->sp] = gmalloc(sizeof(Data));
  t->stack[t->sp]->type = DATA_STR;
  strncpy(t->stack[t->sp]->string.data, "mono", MAXSTR - 1);
  t->sp++;

  t->stack[t->sp] = gmalloc(sizeof(Data));
  t->stack[t->sp]->type = DATA_STR;
  strncpy(t->stack[t->sp]->string.data, "mono", MAXSTR - 1);
  t->sp++;

printf("Stack ptr: %d\n", t->sp);
  loewlib_search(t);
printf("Stack ptr: %d\n", t->sp);
}

#endif
