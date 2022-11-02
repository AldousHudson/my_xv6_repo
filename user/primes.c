#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void prime(int read_fd) {
    // 该子进程读入的第一个质数
    int prime0;

    // 若没有数据传入该子进程，说明已经筛选完毕，exit
    if (read(read_fd, &prime0, sizeof(int)) == 0) {
        exit(0);
    }

    printf("prime %d\n", prime0);

    // 否则创建下一个子进程
    int p[2];
    pipe(p);
    if (fork()) {
        // 当前进程
        close(p[0]);
        int num;
        int flag;
        do {
            flag = read(read_fd, &num, sizeof(int)); // 通过上个进程的管道读取数据，若取得数据flag置1
            if (num % prime0 != 0) {
                write(p[1], &num, sizeof(int)); // 将筛选出来的数据通过当前进程创建的管道写入到下一个子进程
            }
        } while (flag);
        close(p[1]);
        wait((int*)0);
    } else {
        // 下一个子进程
        close(p[1]);
        prime(p[0]); // 下一个子进程调用prime函数从当前进程读取数据，进行递归
        close(p[0]);
    }
    exit(0);
}

int main(int argc, char const *argv[]) {
    if (argc != 1) {
        printf("Primes doesn't need any argument!\n"); // 检查参数数量是否正确
        exit(-1);
    }

    int parent_fd[2];
    pipe(parent_fd);
    if (fork()) {
        // 父进程
        close(parent_fd[0]); // 关闭读端准备写入
        for (int i = 2; i < 36; i++) {
            write(parent_fd[1], &i, sizeof(int));  // 父进程写数据2-35至子进程
        }
        close(parent_fd[1]);
        wait((int*)0);
    } else {
        // 子进程
        close(parent_fd[1]); // 关闭写端准备读取父进程数据
        prime(parent_fd[0]); // prime函数通过读端从父进程读取数据，并开始递归生成子进程
        close(parent_fd[0]);
    }
    exit(0);
}