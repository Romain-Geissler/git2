/*
 * Various trivial helper wrappers around standard functions
 * original source : official git source code
 */
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include "utils.h"
#include "errors.h"
#include "abspath.h"
#include "git-compat-util.h"

static void do_nothing(size_t size)
{
	(void)size;
}

static void (*try_to_free_routine)(size_t size) = do_nothing;

try_to_free_t set_try_to_free_routine(try_to_free_t routine)
{
	try_to_free_t old = try_to_free_routine;
	if (!routine)
		routine = do_nothing;
	try_to_free_routine = routine;
	return old;
}

char *xstrdup(const char *str)
{
	char *ret = strdup(str);
	if (!ret) {
		try_to_free_routine(strlen(str) + 1);
		ret = strdup(str);
		if (!ret)
			die("Out of memory, strdup failed");
	}
	return ret;
}

void *xmalloc(size_t size)
{
	void *ret = malloc(size);
	if (!ret && !size)
		ret = malloc(1);
	if (!ret) {
		try_to_free_routine(size);
		ret = malloc(size);
		if (!ret && !size)
			ret = malloc(1);
		if (!ret)
			die("Out of memory, malloc failed (tried to allocate %lu bytes)",
			    (unsigned long)size);
	}
#ifdef XMALLOC_POISON
	memset(ret, 0xA5, size);
#endif
	return ret;
}

void *xmallocz(size_t size)
{
	void *ret;
	if (unsigned_add_overflows(size, 1))
		die("Data too large to fit into virtual memory space.");
	ret = xmalloc(size + 1);
	((char*)ret)[size] = 0;
	return ret;
}

/*
 * xmemdupz() allocates (len + 1) bytes of memory, duplicates "len" bytes of
 * "data" to the allocated memory, zero terminates the allocated memory,
 * and returns a pointer to the allocated memory. If the allocation fails,
 * the program dies.
 */
void *xmemdupz(const void *data, size_t len)
{
	return memcpy(xmallocz(len), data, len);
}

char *xstrndup(const char *str, size_t len)
{
	char *p = memchr(str, '\0', len);
	return xmemdupz(str, p ? (size_t)(p - str) : len);
}

void *xrealloc(void *ptr, size_t size)
{
	void *ret = realloc(ptr, size);
	if (!ret && !size)
		ret = realloc(ptr, 1);
	if (!ret) {
		try_to_free_routine(size);
		ret = realloc(ptr, size);
		if (!ret && !size)
			ret = realloc(ptr, 1);
		if (!ret)
			die("Out of memory, realloc failed");
	}
	return ret;
}

void *xcalloc(size_t nmemb, size_t size)
{
	void *ret = calloc(nmemb, size);
	if (!ret && (!nmemb || !size))
		ret = calloc(1, 1);
	if (!ret) {
		try_to_free_routine(nmemb * size);
		ret = calloc(nmemb, size);
		if (!ret && (!nmemb || !size))
			ret = calloc(1, 1);
		if (!ret)
			die("Out of memory, calloc failed");
	}
	return ret;
}

/*
 * xread() is the same a read(), but it automatically restarts read()
 * operations with a recoverable error (EAGAIN and EINTR). xread()
 * DOES NOT GUARANTEE that "len" bytes is read even if the data is available.
 */
ssize_t xread(int fd, void *buf, size_t len)
{
	ssize_t nr;
	while (1) {
		nr = read(fd, buf, len);
		if ((nr < 0) && (errno == EAGAIN || errno == EINTR))
			continue;
		return nr;
	}
}

/*
 * xwrite() is the same a write(), but it automatically restarts write()
 * operations with a recoverable error (EAGAIN and EINTR). xwrite() DOES NOT
 * GUARANTEE that "len" bytes is written even if the operation is successful.
 */
ssize_t xwrite(int fd, const void *buf, size_t len)
{
	ssize_t nr;
	while (1) {
		nr = write(fd, buf, len);
		if ((nr < 0) && (errno == EAGAIN || errno == EINTR))
			continue;
		return nr;
	}
}

ssize_t read_in_full(int fd, void *buf, size_t count)
{
	char *p = buf;
	ssize_t total = 0;

	while (count > 0) {
		ssize_t loaded = xread(fd, p, count);
		if (loaded <= 0)
			return total ? total : loaded;
		count -= loaded;
		p += loaded;
		total += loaded;
	}

	return total;
}

ssize_t write_in_full(int fd, const void *buf, size_t count)
{
	const char *p = buf;
	ssize_t total = 0;

	while (count > 0) {
		ssize_t written = xwrite(fd, p, count);
		if (written < 0)
			return -1;
		if (!written) {
			errno = ENOSPC;
			return -1;
		}
		count -= written;
		p += written;
		total += written;
	}

	return total;
}

//int xdup(int fd)
//{
//	int ret = dup(fd);
//	if (ret < 0)
//		die_errno("dup failed");
//	return ret;
//}
//
//FILE *xfdopen(int fd, const char *mode)
//{
//	FILE *stream = fdopen(fd, mode);
//	if (stream == NULL)
//		die_errno("Out of memory? fdopen failed");
//	return stream;
//}
//
//int xmkstemp(char *template)
//{
//	int fd;
//	char origtemplate[PATH_MAX];
//	strlcpy(origtemplate, template, sizeof(origtemplate));
//
//	fd = mkstemp(template);
//	if (fd < 0) {
//		int saved_errno = errno;
//		const char *nonrelative_template;
//
//		if (!template[0])
//			template = origtemplate;
//
//		nonrelative_template = absolute_path(template);
//		errno = saved_errno;
//		die_errno("Unable to create temporary file '%s'",
//			nonrelative_template);
//	}
//	return fd;
//}
//
///* git_mkstemp() - create tmp file honoring TMPDIR variable */
//int git_mkstemp(char *path, size_t len, const char *template)
//{
//	const char *tmp;
//	size_t n;
//
//	tmp = getenv("TMPDIR");
//	if (!tmp)
//		tmp = "/tmp";
//	n = snprintf(path, len, "%s/%s", tmp, template);
//	if (len <= n) {
//		errno = ENAMETOOLONG;
//		return -1;
//	}
//	return mkstemp(path);
//}
//
///* git_mkstemps() - create tmp file with suffix honoring TMPDIR variable. */
//int git_mkstemps(char *path, size_t len, const char *template, int suffix_len)
//{
//	const char *tmp;
//	size_t n;
//
//	tmp = getenv("TMPDIR");
//	if (!tmp)
//		tmp = "/tmp";
//	n = snprintf(path, len, "%s/%s", tmp, template);
//	if (len <= n) {
//		errno = ENAMETOOLONG;
//		return -1;
//	}
//	return mkstemps(path, suffix_len);
//}
//
///* Adapted from libiberty's mkstemp.c. */
//
//#undef TMP_MAX
//#define TMP_MAX 16384
//
//int git_mkstemps_mode(char *pattern, int suffix_len, int mode)
//{
//	static const char letters[] =
//		"abcdefghijklmnopqrstuvwxyz"
//		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
//		"0123456789";
//	static const int num_letters = 62;
//	uint64_t value;
//	struct timeval tv;
//	char *template;
//	size_t len;
//	int fd, count;
//
//	len = strlen(pattern);
//
//	if (len < (size_t)(6 + suffix_len)) {
//		errno = EINVAL;
//		return -1;
//	}
//
//	if (strncmp(&pattern[len - 6 - suffix_len], "XXXXXX", 6)) {
//		errno = EINVAL;
//		return -1;
//	}
//
//	/*
//	 * Replace pattern's XXXXXX characters with randomness.
//	 * Try TMP_MAX different filenames.
//	 */
//	gettimeofday(&tv, NULL);
//	value = ((size_t)(tv.tv_usec << 16)) ^ tv.tv_sec ^ getpid();
//	template = &pattern[len - 6 - suffix_len];
//	for (count = 0; count < TMP_MAX; ++count) {
//		uint64_t v = value;
//		/* Fill in the random bits. */
//		template[0] = letters[v % num_letters]; v /= num_letters;
//		template[1] = letters[v % num_letters]; v /= num_letters;
//		template[2] = letters[v % num_letters]; v /= num_letters;
//		template[3] = letters[v % num_letters]; v /= num_letters;
//		template[4] = letters[v % num_letters]; v /= num_letters;
//		template[5] = letters[v % num_letters]; v /= num_letters;
//
//		fd = open(pattern, O_CREAT | O_EXCL | O_RDWR, mode);
//		if (fd > 0)
//			return fd;
//		/*
//		 * Fatal error (EPERM, ENOSPC etc).
//		 * It doesn't make sense to loop.
//		 */
//		if (errno != EEXIST)
//			break;
//		/*
//		 * This is a random value.  It is only necessary that
//		 * the next TMP_MAX values generated by adding 7777 to
//		 * VALUE are different with (module 2^32).
//		 */
//		value += 7777;
//	}
//	/* We return the null string if we can't find a unique file name.  */
//	pattern[0] = '\0';
//	return -1;
//}
//
//int git_mkstemp_mode(char *pattern, int mode)
//{
//	/* mkstemp is just mkstemps with no suffix */
//	return git_mkstemps_mode(pattern, 0, mode);
//}
//
//int gitmkstemps(char *pattern, int suffix_len)
//{
//	return git_mkstemps_mode(pattern, suffix_len, 0600);
//}
//
//int xmkstemp_mode(char *template, int mode)
//{
//	int fd;
//	char origtemplate[PATH_MAX];
//	strlcpy(origtemplate, template, sizeof(origtemplate));
//
//	fd = git_mkstemp_mode(template, mode);
//	if (fd < 0) {
//		int saved_errno = errno;
//		const char *nonrelative_template;
//
//		if (!template[0])
//			template = origtemplate;
//
//		nonrelative_template = absolute_path(template);
//		errno = saved_errno;
//		die_errno("Unable to create temporary file '%s'",
//			nonrelative_template);
//	}
//	return fd;
//}
//
//static int warn_if_unremovable(const char *op, const char *file, int rc)
//{
//	if (rc < 0) {
//		int err = errno;
//		if (ENOENT != err) {
//			warning("unable to %s %s: %s",
//				op, file, strerror(errno));
//			errno = err;
//		}
//	}
//	return rc;
//}
//
//int unlink_or_warn(const char *file)
//{
//	return warn_if_unremovable("unlink", file, unlink(file));
//}
//
//int rmdir_or_warn(const char *file)
//{
//	return warn_if_unremovable("rmdir", file, rmdir(file));
//}
//
//int remove_or_warn(unsigned int mode, const char *file)
//{
//	return S_ISGITLINK(mode) ? rmdir_or_warn(file) : unlink_or_warn(file);
//}


/*
 * Unlike prefix_path, this should be used if the named file does
 * not have to interact with index entry; i.e. name of a random file
 * on the filesystem.
 */
const char *prefix_filename(const char *pfx, int pfx_len, const char *arg)
{
	static char path[PATH_MAX];
#ifndef WIN32
	if (!pfx_len || is_absolute_path(arg))
		return arg;
	memcpy(path, pfx, pfx_len);
	strcpy(path + pfx_len, arg);
#else
	char *p;
	/* don't add prefix to absolute paths, but still replace '\' by '/' */
	if (is_absolute_path(arg))
		pfx_len = 0;
	else if (pfx_len)
		memcpy(path, pfx, pfx_len);
	strcpy(path + pfx_len, arg);
	for (p = path + pfx_len; *p; p++)
		if (*p == '\\')
			*p = '/';
#endif
	return path;
}

void git2__joinpath_n(char *buffer_out, int count, ...)
{
	va_list ap;
	int i;
	char *buffer_start = buffer_out;

	va_start(ap, count);
	for (i = 0; i < count; ++i) {
		const char *path;
		int len;

		path = va_arg(ap, const char *);

		assert((i == 0) || path != buffer_start);

		if (i > 0 && *path == '/' && buffer_out > buffer_start && buffer_out[-1] == '/')
			path++;

		if (!*path)
			continue;

		len = strlen(path);
		memmove(buffer_out, path, len);
		buffer_out = buffer_out + len;

		if (i < count - 1 && buffer_out[-1] != '/')
			*buffer_out++ = '/';
	}
	va_end(ap);

	*buffer_out = '\0';
}

void git2__joinpath(char *buffer_out, const char *path_a, const char *path_b)
{
	git2__joinpath_n(buffer_out, 2, path_a, path_b);
}
