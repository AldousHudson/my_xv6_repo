#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define MAXLINE 512

int main(int argc, char *argv[]) {
    int exec_index = 0;
    int read_length; // 读取一行字符串的长度
    char buf[MAXLINE];
    char* exec_argv[MAXARG];

    // 复制初始指令，并跳过"xargs"
    for (int i = 1; i < argc; i++) {
        exec_argv[i - 1] = argv[i];
    }
    
    do {
        exec_index = argc - 1;
        memset(exec_argv + exec_index, '\0', sizeof(exec_argv) - exec_index); //保留初始指令在exec_argv中，删除多余的字符串
        
        read_length = read(0, buf, MAXLINE); // 将输入的一行读入buf

        char *arg = (char*) malloc(sizeof(buf)); // 用于存放由输入的一行命令拆分而来的各个字符串
        int index = 0;

        for (int i = 0; i < read_length; i++) {
            if (buf[i] == ' ' || buf[i] == '\n') { //若读到' '或'\n'，说明新发现了一个字符串，将其添加到exec_argv字符串数组m末尾
                arg[index] = '\0';
                index = 0;
                exec_argv[exec_index++] = arg;
                arg = (char*) malloc(sizeof(buf)); //重新分配内存给arg，准备生成新字符串
            } else {
                arg[index++] = buf[i];
            }
        }
        
        if (fork()) {
            wait((int*)0);
        } else {
            // 在子进程中执行命令
            exec(exec_argv[0], exec_argv);
            exit(0);
        }

    } while (read_length > 0);
    
    exit(0);
}