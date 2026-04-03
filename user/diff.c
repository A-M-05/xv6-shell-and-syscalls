// diff.c - simple char-by-char diff for xv6
// Usage:
//   diff file1 file2      -> compare two files
//   diff file             -> compare STDIN with file
 
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "user/user.h"
 
static void
usage(void) {
    fprintf(2, "usage: diff file1 [file2]\n");
}
 
static void
cleanup_and_exit(int fd1, int fd2, int status) {
    if (fd1 > 2)
        close(fd1);
    if (fd2 > 2)
        close(fd2);
    exit(status);
}
 
int
main(int argc, char *argv[]) {
    int fd1, fd2;
 
    if (argc == 2) { // compare stdin (fd 0) with file
        fd1 = 0;
        fd2 = open(argv[1], O_RDONLY);
        if (fd2 < 0) {
            fprintf(2, "diff: cannot open %s\n", argv[1]);
            exit(-1);
        }
    } else if (argc == 3) { // compare two files
        fd1 = open(argv[1], O_RDONLY);
        if (fd1 < 0) {
            fprintf(2, "diff: cannot open %s\n", argv[1]);
            exit(-1);
        }
        fd2 = open(argv[2], O_RDONLY);
        if (fd2 < 0) {
            fprintf(2, "diff: cannot open %s\n", argv[2]);
            close(fd1);
            exit(-1);
        }
    } else {
        usage();
        exit(-1);
    }
 
    // start comparison
    int off = 0;
    char c1, c2;
    for (;;) {
        int n1 = read(fd1, &c1, 1);
        int n2 = read(fd2, &c2, 1);
        if (n1 < 0 || n2 < 0) {
            fprintf(2, "diff: read error\n");
            cleanup_and_exit(fd1, fd2, -1);
        }
        if (n1 == 0 && n2 == 0) // both EOF, equal
            cleanup_and_exit(fd1, fd2, 0);
        if (n1 == 0 && n2 > 0) { // fd1 ended first
            printf("%d\n", off);
            cleanup_and_exit(fd1, fd2, -1);
        }
        if (n1 > 0 && n2 == 0) { // fd2 ended first
            printf("%d\n", off);
            cleanup_and_exit(fd1, fd2, -1);
        }
        if (c1 != c2) { // both have a byte
            printf("%d\n", off);
            cleanup_and_exit(fd1, fd2, -1);
        }
        off++;
    }
}