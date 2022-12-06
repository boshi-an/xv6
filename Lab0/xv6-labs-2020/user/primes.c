#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int first[2];
    pipe(first);
    if(fork() == 0) {
        // child
        close(first[1]);

        int second[2];
        int buf[2];
        int begin = 0, next = 0;
        for(;;) {
            read(first[0], buf, 1);
            if(buf[0] == 0){
                if(next) {
                    write(second[1], buf, 1);
                    wait(0);
                }
                break;
            }
            else if(begin == 0) {
                printf("prime %d\n", buf[0]);
                begin = buf[0];
            }
            else if(buf[0] % begin != 0){
                if(next == 0) {
                    pipe(second);
                    if(fork() == 0) {
                        // child
                        close(second[1]);
                        close(first[0]);
                        first[0] = second[0];
                        begin = 0;
                        // printf("CHILD %d %d\n", first[0], second[0]);
                    }
                    else {
                        // parent
                        next = buf[0];
                        close(second[0]);
                        write(second[1], buf, 1);
                        // printf("PARENT %d %d\n", first[0], second[1]);
                    }
                }
                else {
                    write(second[1], buf, 1);
                }
            }
        }
    }
    else {
        // parent
        close(first[0]);
        int tmp[2];
        for(int i = 2; i <= 35; i++) {
            write(first[1], &i, 1);
        }
        tmp[0] = 0;
        write(first[1], tmp, 1);
        wait(0);
    }
    exit(0);
}