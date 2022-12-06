#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int parent_to_child[2];
    int child_to_parent[2];

    pipe(parent_to_child);
    pipe(child_to_parent);

    if(fork() == 0){
        // child
        int pid = getpid();
        close(parent_to_child[1]);
        close(child_to_parent[0]);

        char buf[64];
        read(parent_to_child[0], buf, 32);
        printf("%d: received ping\n", pid);

        write(child_to_parent[1], "1", 1);
    }
    else {
        // parent
        int pid = getpid();
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        write(parent_to_child[1], "1", 1);

        char buf[64];
        read(child_to_parent[0], buf, 32);
        printf("%d: received pong\n", pid);

        wait(0);
    }

    exit(0);
}