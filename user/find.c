/*
    Usage： find path name
    实现在给定路径的文件夹树中找到全部匹配name的
*/
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
// 注意，这个返回的是一个定长的字符数组，所以如果实际长度不足，后面有一段是空白，如果输出这个或者用这个作比较，是不符合要求
char*
fmtname(char *path)
{
  // 这个函数的作用就是从一个文件的完整路径中取出文件名
  // 比如: ./hello.c -> hello.c
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++; 
  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', DIRSIZ-strlen(p));
  return buf;
}
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
        // 这里不用管原来的path有没有/，对于ls来说，有几个都一样
        *p++ = '/';
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0)
                continue;
            // 计算出当前dirent的路径
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0;
            if(stat(buf, &st) < 0){
                printf("ls: cannot stat %s\n", buf);
                continue;
            }
            // printf("p: %s\tde.name:%s\tbuf:%s\tfmtbuf:%s\n",p,de.name,buf,fmtname(buf));
            if(strcmp(de.name,".")==0||strcmp(de.name,"..")==0){
                continue;
            }
            // if(strcmp(de.name,name)==0){
            //     printf("%s/%s\n",path,name);
            // }
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