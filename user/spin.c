#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int main(){
    int pid=fork();
    if(pid==0){
        while(1)
        printf("child proc\n");
    }else if(pid>0){
        while(1)
        printf("parent proc\n");
    }else{
        fprintf(2,"fork fail\n");
        // exit(-1);
    }
    return 0;
}