/*
 * store.c: switch module that passes on Timber store requests to
 * the appropriate back end.
 */

#include "timber.h"

static const struct storage *current_store = &mbox_store;

char *store_literal(char *message, int msglen, char *separator)
{
    return current_store->store_literal(message, msglen, separator);
}

char *store_retrieve(const char *location, int *msglen, char **separator)
{
    return current_store->store_retrieve(location, msglen, separator);
}

void store_init(void)
{
    current_store->store_init();
}
