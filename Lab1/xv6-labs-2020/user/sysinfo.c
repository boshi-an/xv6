#include "kernel/types.h"
#include "kernel/riscv.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    struct sysinfo info;
    if (sysinfo(&info) < 0)
    {
        printf("FAIL: sysinfo failed");
        exit(1);
    }
    printf("nproc: %d\n", info.nproc);
    printf("freemem: %d\n", info.freemem);
    exit(0);
}