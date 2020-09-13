/*
    Usage： find path name
    实现在给定路径的文件夹树中找到全部匹配name的
*/
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 返回path中最后一个斜杠之后的元素
char *getLastElem(char *p){
    char *t=p;
    char *last=0;
    while(*t!='\0'){
        if(*t=='/'){
            last=t;
        }
        t++;
    }
    // 也可能没有/，那么p指向的文件名
    if(last==0){
        return p;
    }
    return last+1;
}
void find(char* path,char *name){
    // printf("find (%s,,fmt(path):%s,fmtlen:%d,%s)\n",path,fmtname(path),strlen(fmtname(path)),name);
    char buf[512], *p=0;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0){
        fprintf(2, "ls: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){
        fprintf(2, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }
    switch(st.type){
    case T_FILE:
        p=getLastElem(path);
        if(strcmp(p,name)==0)
            printf("%s\n",path);
        break;

    case T_DIR:
        if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
        printf("ls: path too long\n");
        break;
        }
        strcpy(buf, path);
        p = buf+strlen(buf);
        *p++ = '/';
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0)
                continue;
            // 计算出当前dirent的路径
            int t=strlen(de.name)>DIRSIZ?DIRSIZ:strlen(de.name);
            memmove(p, de.name, t);
            p[t] = 0;
            // p让buf这个字符串现在是当前dirent的完整路径
            if(stat(buf, &st) < 0){
                printf("ls: cannot stat %s\n", buf);
                continue;
            }
            if(strcmp(de.name,".")==0||strcmp(de.name,"..")==0){
                continue;
            }
            find(buf,name);
        }
        break;
    }
    close(fd);

}
int main(int argc,char* argv[]){
    // 这里为了简单，假定一定按照usage使用
    // 实际上如果只有一个参数，那么搜索路径为当前路径
    if(argc<3){
        exit();
    }
    find(argv[1],argv[2]);
    exit();

}