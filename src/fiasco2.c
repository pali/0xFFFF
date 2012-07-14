
struct fiasco * fiasco_alloc_empty(void) {

	struct fiasco * fiasco = calloc(1, sizeof(struct fiasco));
	if ( ! fiasco ) {
		perror("Cannot allocate memory");
		return NULL;
	}

	fiasco->fd = -1;
	return fiasco;

}

struct fiasco * fiasco_alloc_from_file(const char * file) {

	struct fiasco * fiasco = fiasco_alloc_empty();
	if ( ! fiasco )
		return NULL;

	fiasco->fd = open(file, O_RDONLY);
	if ( fiasco->fd < 0 ) {
		perror("Cannot open file");
		free(image);
		return NULL;
	}

	/* TODO: Read from file */

	/* use: image_from_fiasco */

}

void fiasco_free(struct fiasco * fiasco) {

	struct image_list * list = fiasco->first;

	while ( list ) {
		struct image_list * next = list->next;
		image_list_del(list);
		list = next;
	}

}

int fiasco_add_image(struct fiasco * fiasco, struct image * image) {

	if ( fiasco->fd >= 0 ) {
		fprintf(stderr, "Fiasco image is on disk\n");
		return 1;
	}

	if ( ! fiasco->first ) {
		fiasco->first = calloc(1, sizeof(struct image_list));
		fiasco->image = image;
	} else {
		image_list_add(&fiasco->first, image);
	}

}

int fiasco_write_to_file(struct fiasco * fiasco, const char * file) {

	/* TODO: Write fiasco image to file */

}

int fiasco_unpack(struct fiasco * fiasco, const char * dir) {

	/* TODO: Unpack fiasco image to dir */

}
