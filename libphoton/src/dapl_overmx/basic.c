#include "photon.h"

int initialize_basic(int nproc, const char *device) {
	return -1;
}

int basic_setup_listeners() {
	return -1;
}

int basic_register_buffer(char *buffer, int buffer_size) {
	return -1;
}

int basic_unregister_buffer(char *buffer) {
	return -1;
}

int basic_post_recv(int proc, char *base, uint32_t offset, uint32_t size, uint32_t *request) {
	return -1;
}

int basic_post_send(int proc, char *base, uint32_t offset, uint32_t size, uint32_t *request) {
	return -1;
}

int basic_wait_all(int remaining_proc, int size) {
	return -1;
}

int basic_wait(uint32_t request) {
	return -1;
}

int basic_wait_remaining() {
	return -1;
}

// the actual photon API
int initialize_photon(int nproc, const char *device) {
	return initialize_basic(nproc, device);
}

int photon_setup_listeners() {
	return basic_setup_listeners();
}

int photon_register_buffer(char *buffer, int buffer_size) {
	return basic_register_buffer(buffer, buffer_size);
}

int photon_unregister_buffer(char *buffer) {
	return basic_unregister_buffer(buffer);
}

int photon_post_recv(int proc, char *base, uint32_t offset, uint32_t size, uint32_t *request) {
	return basic_post_recv(proc, base, offset, size, request);
}

int photon_post_send(int proc, char *base, uint32_t offset, uint32_t size, uint32_t *request) {
	return basic_post_send(proc, base, offset, size, request);
}

int photon_wait_all(int remaining_proc, int size) {
	return basic_wait_all(remaining_proc, size);
}

int photon_wait(uint32_t request) {
	return basic_wait(request);
}

int photon_wait_remaining() {
	return basic_wait_remaining();
}
