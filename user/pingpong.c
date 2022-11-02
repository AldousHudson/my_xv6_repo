#include "kernel/types.h"
#include "user.h"

int main(int argc,char* argv[]) {
    if (argc != 1) {
        printf("Pingpong doesn't need any argument!\n"); // 检查参数数量是否正确
        exit(-1);
    }

    int fd_1[2];
    int fd_2[2];
    int ret1, ret2;
    ret1 = pipe(fd_1); //管道创建后，fd[1]为管道写入端，fd[0]为管道读出端
    ret2 = pipe(fd_2);
    if (ret1 == -1 || ret2 == -1) {
        printf("pipe error\n");
        exit(-2);
    }

    int pid = fork();
    if (pid == 0) {
        // 子进程
        // 读ping
        close(fd_1[1]);
        char msg_from_father[5];
        memset(msg_from_father,'\0',sizeof(msg_from_father));
        int len1 = read(fd_1[0], msg_from_father, sizeof(msg_from_father));
        if (len1 > 0) {
            msg_from_father[len1 - 1] = '\0';
        }
        printf("%d: received %s\n", pid, msg_from_father);

        // 写pong
        close(fd_2[0]);
        char* msg_child = "pong";
        write(fd_2[1], msg_child, strlen(msg_child) + 1);

        exit(0);
    } else if (pid > 0) {
        // 父进程
        // 写ping
        close(fd_1[0]);
        char* msg_father = "ping";
        write(fd_1[1], msg_father, strlen(msg_father) + 1);

        // 读pong
        close(fd_2[1]);
        char msg_from_child[5];
        memset(msg_from_child,'\0',sizeof(msg_from_child));
        int len2 = read(fd_2[0], msg_from_child, sizeof(msg_from_child));
        if (len2 > 0) {
            msg_from_child[len2 - 1] = '\0';
        }
        printf("%d: received %s\n", pid, msg_from_child);

        exit(0);
    }

    exit(0); // 确保进程退出
}