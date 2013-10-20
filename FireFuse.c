/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2013  Karl Lew, <karl@firepick.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall FireFuse.c `pkg-config fuse --cflags --libs` -o firefuse
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

static const char *firefuse_str = "FireFuse Hello!\n";
static const char *firefuse_path = "/picam";

static int firefuse_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (strcmp(path, firefuse_path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(firefuse_str);
	} else
		res = -ENOENT;

	return res;
}

static int firefuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, firefuse_path + 1, NULL, 0);

	return 0;
}

static int firefuse_open(const char *path, struct fuse_file_info *fi)
{
	if (strcmp(path, firefuse_path) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int firefuse_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;
	if(strcmp(path, firefuse_path) != 0)
		return -ENOENT;

	len = strlen(firefuse_str);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, firefuse_str + offset, size);
	} else
		size = 0;

	return size;
}

static struct fuse_operations firefuse_oper = {
	.getattr	= firefuse_getattr,
	.readdir	= firefuse_readdir,
	.open		= firefuse_open,
	.read		= firefuse_read,
};

int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &firefuse_oper, NULL);
}


