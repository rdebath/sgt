/*
 * store.c: switch module that passes on Timber store requests to
 * the appropriate back end.
 */

#include "timber.h"

static const struct storage *current_store = &mbox_store;

char *store_literal(char *message, int msglen)
{
    return current_store->store_literal(message, msglen);
}

char *store_retrieve(const char *location, int *msglen)
{
    return current_store->store_retrieve(location, msglen);
}

void store_init(void)
{
    current_store->store_init();
}
