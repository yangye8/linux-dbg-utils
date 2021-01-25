#include <assert.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <inttypes.h>

struct memtool_fd {
	ssize_t (*write)(struct memtool_fd *handle, off_t offset,
			 const void *buf, size_t nbytes, int width);
	int (*close)(struct memtool_fd *handle);
};

#define container_of(ptr, type, member) \
	(type *)((char *)(ptr) - (char *) &((type *)0)->member)

static off_t mmap_pagesize(void) __attribute__((const));
static off_t mmap_pagesize(void)
{
	static off_t pagesize;

	if (pagesize == 0)
		pagesize = sysconf(_SC_PAGE_SIZE);

	if (pagesize == 0)
		pagesize = 4096;

	return pagesize;
}

struct memtool_mmap_fd {
	struct memtool_fd mfd;
	struct stat s;
	int fd;
};

static ssize_t mmap_write(struct memtool_fd *handle, off_t offset,
			  const void *buf, size_t nbytes, int width)
{
	struct memtool_mmap_fd *mmap_fd =
		container_of(handle, struct memtool_mmap_fd, mfd);
	struct stat *s = &mmap_fd->s;
	off_t map_start, map_off;
	void *map;
	size_t i = 0;
	int ret;

	if (S_ISREG(s->st_mode) && s->st_size < offset + nbytes) {
		ret = posix_fallocate(mmap_fd->fd, offset, nbytes);
		if (ret) {
			errno = ret;
			perror("fallocate");
			return -1;
		}
		s->st_size = offset + nbytes;
	}

	map_start = offset & ~(mmap_pagesize() - 1);
	map_off = offset - map_start;

	map = mmap(NULL, nbytes + map_off, PROT_WRITE,
		   MAP_SHARED, mmap_fd->fd, map_start);
	if (map == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	while (i * width + width <= nbytes) {
		switch (width) {
		case 1:
			((uint8_t *)(map + map_off))[i] = ((uint8_t *)buf)[i];
			break;
		case 2:
			((uint16_t *)(map + map_off))[i] = ((uint16_t *)buf)[i];
			break;
		case 4:
			((uint32_t *)(map + map_off))[i] = ((uint32_t *)buf)[i];
			break;
		case 8:
			((uint64_t *)(map + map_off))[i] = ((uint64_t *)buf)[i];
			break;
		}
		++i;
	}

	ret = munmap(map, nbytes + map_off);
	if (ret < 0) {
		perror("munmap");
		return -1;
	}

	return i * width;
}

static int mmap_close(struct memtool_fd *handle)
{
	struct memtool_mmap_fd *mmap_fd =
		container_of(handle, struct memtool_mmap_fd, mfd);
	int ret;

	ret = close(mmap_fd->fd);

	free(mmap_fd);

	return ret;
}

struct memtool_fd *mmap_open(const char *spec, int flags)
{
	struct memtool_mmap_fd *mmap_fd;
	int ret;

	mmap_fd = malloc(sizeof(*mmap_fd));
	if (!mmap_fd) {
		fprintf(stderr, "Failure to allocate mmap_fd\n");
		return NULL;
	}

	mmap_fd->mfd.write = mmap_write;
	mmap_fd->mfd.close = mmap_close;

	mmap_fd->fd = open(spec, flags, S_IRUSR | S_IWUSR);
	if (mmap_fd->fd < 0) {
		perror("open");
		free(mmap_fd);
		return NULL;
	}

	ret = fstat(mmap_fd->fd, &mmap_fd->s);
	if (ret) {
		perror("fstat");
		close(mmap_fd->fd);
		free(mmap_fd);
		return NULL;
	}

	return &mmap_fd->mfd;
}

void *memtool_open(const char *spec, int flags)
{
	if (!strncmp(spec, "mmap:", 5)) {
		return mmap_open(spec + 5, flags);
	} else if (!strncmp(spec, "mdio:", 5)) {
#ifdef USE_MDIO
		return mdio_open(spec + 5, flags);
#else
		fprintf(stderr, "mdio support not compiled in\n");
		return NULL;
#endif
	} else {
		return mmap_open(spec, flags);
	}
}

ssize_t memtool_write(void *handle,
		      off_t offset, const void *buf, size_t nbytes, int width)
{
	struct memtool_fd *mfd = handle;

	return mfd->write(mfd, offset, buf, nbytes, width);
}

int memtool_close(void *handle)
{
	struct memtool_fd *mfd = handle;

	return mfd->close(mfd);
}

/*
 * Like strtoull() but handles an optional G, M, K or k
 * suffix for Gibibyte, Mibibyte or Kibibyte.
 */
static unsigned long long strtoull_suffix(const char *str, char **endp, int base)
{
	unsigned long long val;
	char *end;

	val = strtoull(str, &end, base);

	switch (*end) {
	case 'G':
		val *= 1024;
	case 'M':
		val *= 1024;
	case 'k':
	case 'K':
		val *= 1024;
		end++;
	default:
		break;
	}

	if (endp)
		*endp = (char *)end;

	return val;
}

static void usage_mw(void)
{
	printf(
"mw - memory write\n"
"\n"
"Usage: mw [-bwlqd] OFFSET DATA...\n"
"\n"
"Write DATA value(s) to the specified REGION.\n"
"\n"
"Options:\n"
"  -b        byte access\n"
"  -w        word access (16 bit)\n"
"  -l        long access (32 bit)\n"
"  -q        quad access (64 bit)\n"
"  -d <FILE> write file (default /dev/mem)\n"
	);
}

int main(int argc, char *argv[])
{
	off_t adr;
	size_t bufsize, size;
	char *buf;
	void *handle;
	int width = 4;
	int opt;
	int i, ret;
	char *file = "/dev/mem";

	while ((opt = getopt(argc, argv, "bwlqd:h")) != -1) {
		switch (opt) {
		case 'b':
			width = 1;
			break;
		case 'w':
			width = 2;
			break;
		case 'l':
			width = 4;
			break;
		case 'q':
			width = 8;
			break;
		case 'd':
			file = optarg;
			break;
		case 'h':
			usage_mw();
			return 0;
		}
	}

	if (optind + 1 >= argc) {
		fprintf(stderr, "Too few parameters for mw\n");
		return EXIT_FAILURE;
	}

	adr = strtoull_suffix(argv[optind++], NULL, 0);

	size = (argc - optind) * width;
	if (!size)
		return EXIT_SUCCESS;

	bufsize = size;
	if (bufsize > 4096)
		bufsize = 4096;

	buf = malloc(bufsize);
	if (!buf) {
		fprintf(stderr, "could not allocate memory\n");
		return EXIT_FAILURE;
	}

	handle = memtool_open(file, O_RDWR | O_CREAT);
	if (!handle)
		return EXIT_FAILURE;

	while (optind < argc) {
		i = 0;

		while (optind < argc && i * width < bufsize) {
			switch (width) {
			case 1:
				((uint8_t *)buf)[i] =
					strtoull(argv[optind], NULL, 0);
				break;
			case 2:
				((uint16_t *)buf)[i] =
					strtoull(argv[optind], NULL, 0);
				break;
			case 4:
				((uint32_t *)buf)[i] =
					strtoull(argv[optind], NULL, 0);
				break;
			case 8:
				((uint64_t *)buf)[i] =
					strtoull(argv[optind], NULL, 0);
				break;
			}
			++i;
			++optind;
		}

		ret = memtool_write(handle, adr, buf, i * width, width);
		if (ret < 0)
			break;

		assert(ret == i * width);
		adr += i * width;
	}


	memtool_close(handle);
	free(buf);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
