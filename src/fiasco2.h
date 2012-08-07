

#ifndef FIASCO_H
#define FIASCO_H

#include "image.h"

struct fiasco {
	char name[257];
	char swver[257];
	int fd;
	struct image_list * first;
}

struct fiasco * fiasco_alloc_empty(void);
struct fiasco * fiasco_alloc_from_file(const char * file);
void fiasco_free(struct fiasco * fiasco);
int fiasco_add_image(struct fiasco * fiasco, struct image * image);
int fiasco_write_to_file(struct fiasco * fiasco, const char * file);
int fiasco_unpack(struct fiasco * fiasco, const char * dir);

#endif
