
#define IMAGE_CHECK(image) do { if ( ! image || image->fd < 0 ) return; } while (0)
#define IMAGE_STORE_CUR(image) do { if ( shared_fd ) image->cur = lseek(image->fd, 0, SEEK_CUR) - image->offset; } while (0)
#define IMAGE_RESTORE_CUR(image) do { if ( shared_fd ) lseek(image->fd, image->offset + image->cur, SEEK_SET); } while (0)

static void image_append(struct image * image, enum image_type type, enum device device, const char * hwrevs, const char * version, const char * layout) {

	image->hash = image_hash(image);
	image->device = device;

	if ( type )
		image->type = type;
	else
		image->type = image_type(image);

	if ( hwrevs )
		image->hwrevs = strdup(hwrevs);

	if ( version )
		image->version = strdup(version);

	if ( layout )
		image->layout = strdup(layout);

}

static struct image * image_alloc(void) {

	struct image * image = calloc(1, sizeof(struct image));
	if ( ! image ) {
		perror("Cannot allocate memory");
		return NULL;
	}
	return image;

}

struct image * image_alloc_from_file(const char * file, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout) {

	struct image * image = image_alloc();
	if ( ! image )
		return NULL;

	image->shared_fd = 0;
	image->fd = open(file, O_RDONLY);
	if ( image->fd < 0 ) {
		perror("Cannot open file");
		free(image);
		return NULL;
	}

	image->size = lseek(image->fd, 0, SEEK_END);
	image->offset = 0;
	image->cur = 0;

	image_append(image, type, device, hwrevs, version, layout);
	return image;

}

struct image * image_alloc_from_fiasco(struct image * image, int fd, size_t size, size_t offset, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout) {

	struct image * image = image_alloc();
	if ( ! image )
		return NULL;

	image->shared_fd = 1;
	image->fd = fd;
	image->size = size;
	image->offset = offset;
	image->cur = 0;

	image_append(image, type, device, hwrevs, version, layout);
	return image;

}

void image_free(struct image * image) {

	IMAGE_CHECK(image);

	if ( image->shared_fd ) {
		close(image->fd);
		image->fd = -1;
	}

	free(image->type);
	free(image->device);
	free(image->hwrevs);
	free(image->version);
	free(image->layout);

	free(image);

}

void image_seek(struct image * image, whence) {

	IMAGE_CHECK(image);
	lseek(image->fd, image->offset + whence, SEEK_SET);
	IMAGE_STORE_CUR(image)

}

size_t image_read(struct image * image, void * buf, size_t count) {

	IMAGE_CHECK(image);
	IMAGE_RESTORE_CUR(image);

	if ( image->cur + count > image->size )
		count = image->size - image->cur;

	count = read(image->fd, buf, count);

	IMAGE_STORE_CUR(image);
	return count;

}

size_t image_write(struct image * image, void * buf, size_t count) {

	IMAGE_CHECK(image);
	IMAGE_RESTORE_CUR(image);
	count = write(image->fd, buf, count);
	IMAGE_STORE_CUR(image);
	return count;

}


void image_list_add(struct image_list ** list, struct image * image) {

	struct image_list * first = calloc(1, sizeof(struct image_list));
	if ( ! next )
		return;

	first->image = image;
	first->next = *list;

	if ( *list ) {
		first->prev = *list->prev;
		if ( *list->prev )
			*list->prev->next = first;
		*list->prev = first;
	} else {
		*list = first;
	}

}

void image_list_del(struct image_list * list) {

	if ( ! list )
		return;

	if ( list->prev )
		list->prev->next = list->next;

	if ( list->next )
		list->next->prev = list->prev;

	image_free(list->image);
	free(list);

}
