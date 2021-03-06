#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "lib/mlr_arch.h"
#include "lib/mlrutil.h"
#include "lib/mlr_globals.h"
#include "file_reader_mmap.h"

#if MLR_ARCH_MMAP_ENABLED
static char empty_buf[1] = { 0 };
#endif

// ----------------------------------------------------------------
file_reader_mmap_state_t* file_reader_mmap_open(char* prepipe, char* file_name) {
#if MLR_ARCH_MMAP_ENABLED
	// popen is a stdio construct, not an mmap construct, and it can't be supported here.
	if (prepipe != NULL) {
		fprintf(stderr, "%s: coding error detected in file %s at line %d.\n",
			MLR_GLOBALS.bargv0, __FILE__, __LINE__);
		exit(1);
	}

	file_reader_mmap_state_t* pstate = mlr_malloc_or_die(sizeof(file_reader_mmap_state_t));
	pstate->fd = open(file_name, O_RDONLY);
	if (pstate->fd < 0) {
		perror("open");
		fprintf(stderr, "%s: could not open \"%s\"\n", MLR_GLOBALS.bargv0, file_name);
		exit(1);
	}
	struct stat stat;
	if (fstat(pstate->fd, &stat) < 0) {
		perror("fstat");
		fprintf(stderr, "%s: could not fstat \"%s\"\n", MLR_GLOBALS.bargv0, file_name);
		exit(1);
	}
	if (stat.st_size == 0) {
		// mmap doesn't allow us to map zero-length files but zero-length files do exist.
		pstate->sol = &empty_buf[0];
	} else {
		pstate->sol = mmap(NULL, (size_t)stat.st_size, PROT_READ|PROT_WRITE, MAP_FILE|MAP_PRIVATE, pstate->fd, (off_t)0);
		if (pstate->sol == MAP_FAILED) {
			perror("mmap");
			fprintf(stderr, "%s: could not mmap \"%s\"\n", MLR_GLOBALS.bargv0, file_name);
			exit(1);
		}
	}
	pstate->eof = pstate->sol + stat.st_size;
	// POSIX semantics: the mmap itself increments a reference count to the file, in addition to the
	// open.  We close the file but keep the mmap reference until a subsequent munmap.
	if (close(pstate->fd) < 0) {
		perror("close");
		exit(1);
	}
	return pstate;
#else
	fprintf(stderr, "%s: mmap is unsupported on this architecture.\n", MLR_GLOBALS.bargv0);
	exit(1);
	return NULL;
#endif
}

// ----------------------------------------------------------------
// Here we intentionally do not munmap.
//
// This method is used by various lrec readers, where lrecs are instantiated with keys/values
// pointing into mmapped file-contents buffers.  This is done for the sake of performance, to reduce
// data-copies. But it also means we can't unmap files after ingesting lrecs, since the lrecs in
// question might be retained after the input-file closes.  Example: mlr sort on multiple files.
void file_reader_mmap_close(file_reader_mmap_state_t* pstate, char* prepipe) {
	free(pstate);
}

// ----------------------------------------------------------------
void* file_reader_mmap_vopen(void* pvstate, char* prepipe, char* file_name) {
	return file_reader_mmap_open(prepipe, file_name);
}

// ----------------------------------------------------------------
void file_reader_mmap_vclose(void* pvstate, void* pvhandle, char* prepipe) {
	file_reader_mmap_close(pvhandle, prepipe);
}
