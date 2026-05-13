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

void generate_tujuan(char *content){
    strcpy(content, "Tujuan Mas Amba: ");


    for (int i = 1; i <= 7; i++ ){
        char filepath[1024];
        sprintf(filepath, "%s/%d.txt", source_dir, i);
        FILE *f = fopen(filepath, "r");

        if(f){
            char line[256];
            while (fgets(line, sizeof(line), f)){
                if (strncmp(line, "KOORD: ", 7)==0){
                    
                    char frag[256];
                    strcpy(frag, line + 7);

                    size_t len = strlen(frag);
                    if(len > 0 && frag[len-1] == '\n') frag[len-1] = '\0';
                    if(len > 1 && frag[len-2] == '\r') frag[len-2] = '\0';

                    strcat(content, frag);
                    break;

                }
                
            }
            fclose(f);
        }

    }
    strcat(content, "\n");
}

static int xmp_getattr(const char *path, struct stat *stbuf) {
    int res = 0;
    memset(stbuf, 0, sizeof(struct stat));


    if (strcmp(path, "/tujuan.txt") == 0) {
        stbuf->st_mode = S_IFREG | 0444; 
        stbuf->st_nlink = 1;
        
        char content[4096];
        generate_tujuan(content);
        stbuf->st_size = strlen(content); 
        return 0;
    }


    char fpath[1000];
    sprintf(fpath, "%s%s", source_dir, path);
    res = lstat(fpath, stbuf);

    if (res == -1) return -errno;
    return 0;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    char fpath[1000];
    
    if (strcmp(path, "/") == 0) {
        sprintf(fpath, "%s", source_dir);
    } else {
        sprintf(fpath, "%s%s", source_dir, path);
    }

    DIR *dp;
    struct dirent *de;
    (void) offset;
    (void) fi;

    dp = opendir(fpath);
    if (dp == NULL) return -errno;

    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        memset(&st, 0, sizeof(st));
        st.st_ino = de->d_ino;
        st.st_mode = de->d_type << 12;

        if (filler(buf, de->d_name, &st, 0)) break;
    }
    closedir(dp);


    if (strcmp(path, "/") == 0) {
        filler(buf, "tujuan.txt", NULL, 0);
    }

    return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        return 0; 
    }

    char fpath[1000];
    sprintf(fpath, "%s%s", source_dir, path);
    int res = open(fpath, fi->flags);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    if (strcmp(path, "/tujuan.txt") == 0) {
        char content[4096];
        generate_tujuan(content);
        
        size_t len = strlen(content);
        if (offset < len) {
            if (offset + size > len) size = len - offset;
            memcpy(buf, content + offset, size);
        } else {
            size = 0;
        }
        return size;
    }

    char fpath[1000];
    sprintf(fpath, "%s%s", source_dir, path);
    
    int fd = open(fpath, O_RDONLY);
    if (fd == -1) return -errno;
    
    int res = pread(fd, buf, size, offset);
    if (res == -1) res = -errno;
    
    close(fd);
    return res;
}

static struct fuse_operations xmp_oper = {
    .getattr = xmp_getattr,
    .readdir = xmp_readdir,
    .open = xmp_open,
    .read = xmp_read,
};


int main(int argc, char *argv[]){

    if (argc < 3){
        printf("Usage: %s <source_directory> <mount_directory>\n", argv[0]);
        return 1;
    }

    if (realpath(argv[1], source_dir) == NULL) {
        perror("Error resolving source directory path");
        return 1;
    }

    char *fuse_argv[] = {argv[0], argv[2], NULL};
    int fuse_argc = 2;

    umask(0);
    return fuse_main(fuse_argc, fuse_argv, &xmp_oper, NULL);
}