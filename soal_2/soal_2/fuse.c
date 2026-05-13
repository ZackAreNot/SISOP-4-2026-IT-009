#define FUSE_USE_VERSION 28
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>

char source_dir[1024];

void cipher(char *buf, size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] ^= 0x76;
    }
}

static int xmp_getattr(const char *path, struct stat *stbuf) {
    char fpath[1000];
    sprintf(fpath, "%s%s", source_dir, path);
    
    if (lstat(fpath, stbuf) == -1) {

        sprintf(fpath, "%s%s.enc", source_dir, path);
        if (lstat(fpath, stbuf) == -1) {
            return -errno;
        }

    }
    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    char fpath[1000];
    sprintf(fpath, "%s%s", source_dir, path);
    
    DIR *dp = opendir(fpath);
    if (dp == NULL) return -errno;

    struct dirent *de;
    (void) offset;
    (void) fi;

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        char entry_name[256];
        strcpy(entry_name, de->d_name);
        
        int len = strlen(entry_name);
        if (len > 4 && strcmp(entry_name + len - 4, ".enc") == 0) {
            entry_name[len - 4] = '\0';
        }
        
        if (filler(buf, entry_name, &st, 0)) break;
    }
    closedir(dp);
    return 0;
}

static int xmp_mkdir(const char *path, mode_t mode) {
    char fpath[1000];
    sprintf(fpath, "%s%s", source_dir, path);
    int res = mkdir(fpath, mode);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_rmdir(const char *path) {
    char fpath[1000];
    sprintf(fpath, "%s%s", source_dir, path);
    int res = rmdir(fpath);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char fpath[1000];
    sprintf(fpath, "%s%s.enc", source_dir, path);
    int res = creat(fpath, mode);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    char fpath[1000];
    sprintf(fpath, "%s%s.enc", source_dir, path);
    int res = open(fpath, fi->flags);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char fpath[1000];
    sprintf(fpath, "%s%s.enc", source_dir, path);
    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;
    
    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    else if (res > 0) cipher(buf, res); 
    
    close(fd);
    return res;
}

static int xmp_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char fpath[1000];
    sprintf(fpath, "%s%s.enc", source_dir, path);
    int fd = open(fpath, O_WRONLY);
    if (fd == -1) return -errno;

    char *enc_buf = malloc(size);
    memcpy(enc_buf, buf, size);
    cipher(enc_buf, size); 

    int res = pwrite(fd, enc_buf, size, offset);
    if (res == -1) res = -errno;
    
    free(enc_buf);
    close(fd);
    return res;
}

static int xmp_truncate(const char *path, off_t size) {
    char fpath[1000];
    sprintf(fpath, "%s%s.enc", source_dir, path);
    int res = truncate(fpath, size);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_unlink(const char *path) {
    char fpath[1000];
    sprintf(fpath, "%s%s.enc", source_dir, path);
    int res = unlink(fpath);
    if (res == -1) return -errno;
    return 0;
}

static int xmp_access(const char *path, int mask) {
    char fpath[1000];
    sprintf(fpath, "%s%s", source_dir, path);
    if (access(fpath, mask) == -1) {
        sprintf(fpath, "%s%s.enc", source_dir, path);
        if (access(fpath, mask) == -1) return -errno;
    }
    return 0;
}

static int xmp_utimens(const char *path, const struct timespec tv[2]) {
    char fpath[1000];
    sprintf(fpath, "%s%s", source_dir, path);
    if (access(fpath, F_OK) == -1) {
        sprintf(fpath, "%s%s.enc", source_dir, path);
    }
    int res = utimensat(0, fpath, tv, AT_SYMLINK_NOFOLLOW);
    if (res == -1) return -errno;
    return 0;
}

static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .mkdir = xmp_mkdir,
    .rmdir = xmp_rmdir,
    .create = xmp_create,
    .open = xmp_open,
    .read = xmp_read,
    .write = xmp_write,
    .truncate = xmp_truncate,
    .unlink = xmp_unlink,
    .access = xmp_access,
    .utimens = xmp_utimens,
};

int main(int argc, char *argv[]) {

    if (argc < 3) {
        printf("Usage: %s <encrypted_storage> <fuse_mount>\n", argv[0]);
        return 1;
    }
    
    if (realpath(argv[1], source_dir) == NULL) {
        perror("Error resolving source directory path");
        return 1;
    }
    
    char *fuse_argv[] = {argv[0], "-o", "allow_other", argv[2], NULL};
    int fuse_argc = 4;
    
    umask(0);
    return fuse_main(fuse_argc, fuse_argv, &xmp_oper, NULL);
}