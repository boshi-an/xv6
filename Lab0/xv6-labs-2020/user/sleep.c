#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if(argc<2 || argc>2){
        fprintf(2, "Usage: sleep <number of seconds> \n");
    }
    else{
        int ticks = atoi(argv[1]);
        sleep(ticks);
    }
    exit(0);
}