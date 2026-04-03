#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "user/user.h"
 
#define PATH_SZ 512
static char path_buffer[PATH_SZ];
static int path_buffer_len = 0;
 
/** @brief Find own name in parent file descriptor.
 *  @param  pfd   : parent file descriptor
 *  @param  inum  : my own inode
 *  @param  name  : where to put the name
 *  @return the size of the name, or -1 if error.
 */
static int lookup(int pfd, uint inum, char *name) {
    struct dirent de;
    // read one entry at the time from the parent file descriptor
    while (read(pfd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0)
            continue;
        // if the entry is "." or "..", skip it.
        if (de.name[0]=='.' &&
            (de.name[1]==0 || (de.name[1]=='.' && de.name[2]==0)))
            continue;
        // if inum found, then copy the name
        if (de.inum == inum) {
            int n = 0;
            while (n < DIRSIZ && de.name[n]) {
                name[n] = de.name[n];
                n++;
            }
            name[n] = 0;
            return n;
        }
    }
    return -1;
}
 
/** @brief Prepend the given name to the global path_buffer
 *  @param  name     : buffer with the given name ('\0' ended).
 *  @param  name_len : length of the name (including '\0')
 *  @return 0 if ok, -1 if error
  */
static int prepend(const char *name, int name_len) {
    // if path_buffer cannot fit a "/" and the new name, error
    if (PATH_SZ <= 1 + name_len + path_buffer_len) return -1;
    // right-shift path_buffer by (path_buffer_len + 1) characters
    memmove(path_buffer + 1 + name_len, path_buffer, path_buffer_len + 1);
    // prefix the name
    memmove(path_buffer + 1, name, name_len);
    // and add the '/' at the beginning.
    path_buffer[0] = '/';
    // update the path length plus the "/"
    path_buffer_len += name_len + 1;
    return 0;
}
 
int main(void) {
    struct stat st, pst;
    int pfd;
    char dir_name[DIRSIZ+1];
    // clear path buffer
    memset(path_buffer, 0, PATH_SZ);
 
    // go up in the directory structure finding the root.
    for (;;) {
        // try to get stats on current and parent directory
        if (stat(".",  &st)  < 0 ||
            stat("..", &pst) < 0) goto fail;
 
        // if we cannot keep going up (the inodes of this and parent
        // directories are the same), then we reached the root
        if (st.ino == pst.ino) {
            // if path_buffer[0] == 0, then we started at the root
            // otherwise, print the path.
            printf("%s\n", path_buffer[0] ? path_buffer : "/");
            goto ok;
        }
 
        // lookup current dir's name in parent dir
        pfd = open("..", O_RDONLY);
        if (pfd < 0) goto fail;
        int n = lookup(pfd, st.ino, dir_name);
        close(pfd);
        if (n < 0) goto fail;
        // try to build path from the front
        if (prepend(dir_name, n)) goto fail;
        // try to go one level up
        if (chdir("..") < 0) goto fail;
    }
fail:
    fprintf(2, "pwd: error\n");
    return -1;
ok:
    return 0;
}