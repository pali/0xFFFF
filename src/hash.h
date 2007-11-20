#ifndef _INCLUDE_HASH_H_
#define _INCLUDE_HASH_H_

#define usho unsigned short
#define BSIZE 0x20000

usho do_hash(usho *b, int len);
usho do_hash_file(const char *filename);

#endif
