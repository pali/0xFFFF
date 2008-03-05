#ifndef _OS_H_
#define _OS_H_

#if __linux__ || __NetBSD__ || __FreBSD__ || __OpenBSD__ || __Darwin__ || __MacOSX__
#define HAVE_SQUEUE 1
#else
#define HAVE_SQUEUE 0
#endif

#endif
