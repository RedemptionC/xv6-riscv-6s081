#include "user/user.h"

void Usage(){
    printf("Usage: uptime <NUM>\n");
    printf("output how many ticks passed during this execution\n");
    exit();
}
int main(int argc,char *argv[]){
    if(argc<2){
        Usage();
    }
    char c=argv[1][0];
    if(c<'0'||c>'9'){
        Usage();
    }
    int time=atoi(argv[1]);
    int total=uptime();
    sleep(time);
    printf("%d ticks passed in sleep(%d)\n",total,time);
    exit();
}