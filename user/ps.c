#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
 
const char *states_str[] = {
        "RUNNING  : ",
        "RUNNABLE : ",
        "SLEEPING : ",
        "ZOMBIE   : "
};

int counters[4];

int main(int argc, char **argv)
{
    if(ps(counters)) {
        fprintf(2, "System call ps failed.\n");
        exit(-1);
    }
    for(int i=0; i<4; i++){
        printf("%s%d\n", states_str[i], counters[i]);
    }
    return 0;
}