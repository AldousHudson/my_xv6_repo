#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char* fmtname(char *path) {
    char *p;

     // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;
    return p;
}

void find(char *path, char *name) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    // 打开文件（夹）
    if ((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    // 读取文件（夹）信息
    if (fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type) {
    case T_FILE: // 如果是文件，则检查文件名
        if (strcmp(fmtname(path), name) == 0) {
            printf("%s\n", path);
        }
        break;
    
    case T_DIR: // 如果是文件夹，则递归查找
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof(buf)) {
            printf("find: path too long\n");
            break;
        }

        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        
        //每次读取一个文件，直到读取完毕
        while (read(fd, &de, sizeof(de)) == sizeof(de)) {
            if (de.inum == 0) {
                continue;
            }

            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            
            // 跳过子文件夹中的.和..
            if (!strcmp(de.name, ".") || !strcmp(de.name, "..")) {
                continue;
            }

            if(stat(buf, &st) < 0){
                printf("find: cannot stat %s\n", buf);
                continue;
            }

            find(buf, name);
        }
        break;
    }
    close(fd);
}

int main(int argc,char* argv[]) {
    if (argc != 3) {
        printf("Find needs two arguments!\n"); // 检查参数数量是否正确
        exit(-1);
    }
    find(argv[1], argv[2]);
    exit(0);
}