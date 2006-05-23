#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <grp.h>
#include <stdio.h>
#include <unistd.h>

/**
 * Is a file executable (by execv() and co.)?
 *
 * Note that the file must be readable, and executable for execv() to
 * work.  Also user, group and other permissions are used with no fall
 * through: e.g. if we are the file owner then the user permissions
 * determine access, irrespective of the group and other permissions.
 * Again, this mimics execv().
 */
int is_executable(const char *pathname)
{
    gid_t extragids[NGROUPS_MAX];
    int i, n;
    struct stat st;

    if (stat(pathname, &st) < 0) {
        return 0;
    }

    /* user access */
    if ((getuid() == st.st_uid) || (geteuid() == st.st_uid)) {
        return (st.st_mode & S_IRUSR) && (st.st_mode & S_IXUSR);
    }

    /* group access -- include extra groups */
    if ((getgid() == st.st_gid) || (getegid() == st.st_gid)) {
        return (st.st_mode & S_IRGRP) && (st.st_mode & S_IXGRP);
    }
    if ((n = getgroups(NGROUPS_MAX, extragids)) < 0) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (extragids[i] == st.st_gid) {
            return (st.st_mode & S_IRGRP) && (st.st_mode & S_IXGRP);
        }
    }

    /* other access */
    return (st.st_mode & S_IROTH) && (st.st_mode & S_IXOTH);
}


int main(int argc, char *argv[])
{
    if (is_executable(argv[1])) {
        puts("executable");
    } else {
        puts("not executable");
    }
    return 0;
}
