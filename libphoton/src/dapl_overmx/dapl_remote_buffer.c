#include <stdlib.h>
#include <string.h>

#include "dapl.h"
#include "dapl_remote_buffer.h"

dapl_remote_buffer_t *dapl_remote_buffer_create() {
	dapl_remote_buffer_t *drb;

	drb = malloc(sizeof(dapl_remote_buffer_t));
	if (!drb)
		return NULL;

	bzero(drb, sizeof(*drb));

	drb->request = NULL_COOKIE;

	return drb;
}

void dapl_remote_buffer_free(dapl_remote_buffer_t *drb) {
	free(drb);
}
