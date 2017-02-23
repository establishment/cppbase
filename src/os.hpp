#ifndef CPP_BASE_OS_HPP
#define CPP_BASE_OS_HPP

#include <fcntl.h>
#include <ftw.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "logger.hpp"
#include "string_utils.hpp"

#define UNUSED __attribute__((unused))

namespace Base {
int DirExists(const std::string& path) {
    struct stat st;
    return (stat(path.c_str(), &st) >= 0 && S_ISDIR(st.st_mode));
}

int PathBeginsWith(const std::string& path, const std::string& with) {
    return with == path.substr(0, with.size());
}

void MakeDir(const std::string& s_path, int mode = 0777) {
    char* path = strdup(s_path.c_str());

    int len = strlen(path);

    if (path[len - 1] == '/') {
        path[len - 1] = 0;
    }

    for (char* ch = (*path == '/') ? (path + 1) : (path); *ch; ch++) {
        if (*ch == '/') {
            *ch = 0;
            if (!DirExists(path) && mkdir(path, mode) < 0) {
                Die("Cannot create directory %s: %m\n", path);
            }
            *ch = '/';
        }
    }

    if (!DirExists(path) && mkdir(path, mode) < 0) {
        Die("Cannot create directory %s: %m\n", path);
    }
}

void Touch(std::string path) {
    if (path.size() == 0) {
        return;
    }

    int fd = open(path.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
    if (fd != -1) {
        close(fd);
    }
}

int RMTreeHelper(const char* fpath, const struct stat* sb, int typeflag UNUSED, struct FTW* ftwbuf UNUSED) {
    if (S_ISDIR(sb->st_mode)) {
        if (rmdir(fpath) < 0) {
            Die("Cannot rmdir %s: %m", fpath);
        }
    } else {
        if (unlink(fpath) < 0) {
            Die("Cannot unlink %s: %m", fpath);
        }
    }
    return FTW_CONTINUE;
}

void RMTree(const char* path) {
    nftw(path, RMTreeHelper, 32, FTW_MOUNT | FTW_PHYS | FTW_DEPTH);
}

static uid_t chownUid;
static gid_t chownGid;

int ChownTreeHelper(const char* fpath, const struct stat* sb UNUSED, int typeflag UNUSED, struct FTW* ftwbuf UNUSED) {
    if (lchown(fpath, chownUid, chownGid) < 0) {
        Die("Cannot chown %s: %m", fpath);
    } else {
        return FTW_CONTINUE;
    }
}

void ChownTree(const char* path, uid_t uid, gid_t gid) {
    chownUid = uid;
    chownGid = gid;
    nftw(path, ChownTreeHelper, 32, FTW_MOUNT | FTW_PHYS);
}

void RChmod(const std::string& command) {
    int err = system(StrCat("/bin/chmod -R ", command).c_str());
    if (err == -1) {
        Die("RChmod %s\n%m\n", command.c_str());
    }
}

void Chmod(const std::string& command) {
    int err = system(StrCat("/bin/chmod ", command).c_str());
    if (err == -1) {
        Die("Chmod %s\n%m\n", command.c_str());
    }
}

void Setfacl(const std::string& command) {
    if (access("/usr/bin/setfacl", F_OK) == -1) {
        Die("setfacl is not present on system.\n apt install acl");
    }
    /// create a file lock so no 2 processes set the acl
    /// at the same time because it crashes sometime.
    /// magic.
    /* l_type   l_whence  l_start  l_len  l_pid   */
    struct flock fl = {F_WRLCK, SEEK_SET, 0, 0, getpid()};
    int fd;

    fl.l_pid = getpid();

    if ((fd = open("setfacl_lock", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
        perror("open");
        exit(1);
    }

    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        perror("fcntl");
        exit(1);
    }

    int err = system(StrCat("/usr/bin/setfacl ", command).c_str());

    fl.l_type = F_UNLCK; /* set to unlock same region */

    if (fcntl(fd, F_SETLK, &fl) == -1) {
        perror("fcntl");
        exit(1);
    }

    if (err != 0) {
        Die("Setfacl %s\n%m\n", command.c_str());
    }
}

}  // namespace Base

#endif
