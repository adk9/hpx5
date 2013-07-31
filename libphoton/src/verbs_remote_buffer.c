#include <stdlib.h>
#include <string.h>

#include "verbs.h"
#include "verbs_remote_buffer.h"

verbs_remote_buffer_t *verbs_remote_buffer_create() {
	verbs_remote_buffer_t *drb;

	drb = malloc(sizeof(verbs_remote_buffer_t));
	if (!drb)
		return NULL;

	bzero(drb, sizeof(*drb));

	drb->request = NULL_COOKIE;

	return drb;
}

void verbs_remote_buffer_free(verbs_remote_buffer_t *drb) {
	free(drb);
}
