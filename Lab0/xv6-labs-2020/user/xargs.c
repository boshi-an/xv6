#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

int main(int argc, char *argv[])
{
    int MX = 128;
    char line[MX];
    while(1)
    {
        gets(line, MX);

        if(line[0] == 0) break;
        else line[strlen(line)-1] = 0;

        if(fork() == 0)
        {
            // child
            char full_command[MX];
            char *q = full_command;
            for(int j=1; j<argc; j++)
            {
                strcpy(q, argv[j]);
                q += strlen(argv[j]);
                *q = ' ';
                q++;
            }
            strcpy(q, line);
            q += strlen(line);
            *q = 0;

            char *args[MX];
            int cnt = 0;
            char *p = full_command;
            while(*p)
            {
                while(*p == ' ') p++;
                if(*p == 0) break;
                args[cnt++] = p;
                while(*p != ' ' && *p != 0) p++;
                if(*p == 0) break;
                *p = 0;
                p++;
            }
            exec(full_command, args);
            exit(0);
        }
        else
        {
            // parent
            wait(0);
        }
    }
    exit(0);
}