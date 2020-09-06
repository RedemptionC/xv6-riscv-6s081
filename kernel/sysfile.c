//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;
  // argxx 类底层都是调用的argraw，使用p->tf->ax，即使用寄存器传递参数
  if(argint(n, &fd) < 0)
    return -1;
  // 判断fd的范围，以及是否是当前程序打开的文件
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  // 用指针参数设置返回值：该fd和关联的文件
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    // 从当前进程ofile中，找一个没有打开的fd，分配出去
    // 将该fd与作为参数的文件关联起来
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;
  // 首先取得old fd关联的文件
  if(argfd(0, 0, &f) < 0)
    return -1;
  // 为该文件再分配一个与之关联的fd：p->ofile[x],p->ofile[y]都为f
  if((fd=fdalloc(f)) < 0)
    return -1;
  // 增加f的ref
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;
  // 获取参数：文件，目的地址，读取字节数
  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;
  // 获取参数：文件，源地址，写字节数
  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;
  // 获取要关闭的文件和fd
  if(argfd(0, &fd, &f) < 0)
    return -1;
  // 将当前进程ofile[fd]设为0
  myproc()->ofile[fd] = 0;
  // 
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  if(argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
uint64
sys_link(void)
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;
  // 获取两个字符串参数：新路径，老路径
  // 新路径作为一个连接指向老路径
  if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
    return -1;

  begin_op(ROOTDEV);
  // 获取到old路径中最后一个元素的inode
  if((ip = namei(old)) == 0){
    end_op(ROOTDEV);
    return -1;
  }
  // 最后一个元素不能是文件夹
  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op(ROOTDEV);
    return -1;
  }
  // 增加nlink
  ip->nlink++;
  iupdate(ip);
  iunlock(ip);
  
  // 获取new path对应文件的文件夹的inode，将name设置为对应文件，即路径中的最后一个元素
  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  // 必须是在同一个设备（磁盘）上,如果不等于，那么短路原则，判断结束，直接bad
  // 如果等于，使用dirlink，在name所在的文件夹创建一个dirent
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op(ROOTDEV);

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op(ROOTDEV);
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    // 要判断文件夹为空，只要所有dirent（除了第一第二个分别是.,..)的inum为0
    // TODO 为0是什么时候设置的？
    // 👆应该是memset为0，所以inum也是0
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

uint64
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  if(argstr(0, path, MAXPATH) < 0)
    return -1;

  begin_op(ROOTDEV);
  // 获取该文件所在文件夹的inode，设置name为该文件的文件名（path中最后一个元素
  if((dp = nameiparent(path, name)) == 0){
    end_op(ROOTDEV);
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;
  // 找不到，就bad，如果找到了，将ip指向该文件的inode
  // 这里还设置了off，off指向name这个文件在当前文件夹的dirent里的offset
  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  // 不能unlink非空文件夹
  // TODO 但是貌似link本身也不能link文件夹？
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  // de里面不是空的吗 √
  // 这里调用writei，将de的内容写入到dp（文件夹的inode）中
  // 👆所以这里实际上就是在原来dirent的位置写一个空的dirent
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  // √ 如果ip指向的是文件夹 就减少文件夹的ilink？此时ip和dp是一个inode吗
  // 👆 原因在于，ip是文件夹，那么其中的一个dirent: .. ,指向ip对应文件所在的
  // 文件夹，ip被unlink了，那么他的dirent .. ，自然也没有了，所以对他所在 
  // 文件夹的link减少了
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op(ROOTDEV);

  return 0;

bad:
  iunlockput(dp);
  end_op(ROOTDEV);
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];
  // 获取path对应文件所在文件夹的inode，name设置为文件名
  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);
  // ip指向name对应的inode（是dp这个文件夹下的dirent）
  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    // 如果这个文件已经存在了，直接返回
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }
  // 分配一个inode，ip指向它
  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
  // 如果创建的文件本身是文件夹，那么dp->nlink++,因为文件夹内一定有个dirent是..,它指向自己所在的文件夹
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    // 这里还是利用短路，如果错误，就会返回-1，那么后面的就不会进行
    // 这里是如果创建的是文件夹，那么为他创建.,..，两个必备的dirent
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }
  // 再把创建的文件（夹），在他所在的目录中创建对应的dirent
  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");
  // 注意这里返回的是ip，仍然上锁
  iunlockput(dp);

  return ip;
}

struct inode *
symfollow(struct inode * ip){
  int symlinks=1;
  struct inode *t;
  while(1){
    t=namei(ip->target);
    if(t==0){
      iunlock(ip);
      return 0;
    }
    iunlock(ip);
    ilock(t);
    if(t->type!=T_SYMLINK){
      break;
    }
    ip=t;
    symlinks++;
    if(symlinks>10){
      return 0;
    }
  }
  return t;
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;
  // 路径和打开模式
  if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op(ROOTDEV);

  if(omode & O_CREATE){
    // 如果mode里有create，那么直接调用create
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op(ROOTDEV);
      return -1;
    }
  } else {
    // 不创建，那么就是读/写（当然还有新增的nofollow
    // 拿到path对应文件的inode
    if((ip = namei(path)) == 0){
      end_op(ROOTDEV);
      return -1;
    }
    ilock(ip);
    // 不能用open打开文件夹（看了下之前lab util，我用的是read读取dirent
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op(ROOTDEV);
      return -1;
    }
    if(ip->type == T_SYMLINK && (omode & O_NOFOLLOW)==0){
      ip=symfollow(ip);
      if(ip==0){
        end_op(ROOTDEV);
        return -1;
      }
    }
  }
  // 如果是设备，那么设备号要合法
  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    iunlockput(ip);
    end_op(ROOTDEV);
    return -1;
  }
  // 如果分配文件成功了，就再分配一个fd，如果任一个失败了，就error，有必要的话关闭刚分配的文件
  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op(ROOTDEV);
    return -1;
  }
  // 后面就是设置相关属性，type，如果是设备就要设置设备号，然后设置文件的inode，off，可读，可写等
  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
    f->minor = ip->minor;
  } else {
    f->type = FD_INODE;
  }
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  iunlock(ip);
  end_op(ROOTDEV);

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[MAXPATH];
  struct inode *ip;

  begin_op(ROOTDEV);
  // 直接调用create
  if(argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op(ROOTDEV);
    return -1;
  }
  iunlockput(ip);
  end_op(ROOTDEV);
  return 0;
}

uint64
sys_mknod(void)
{
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;

  begin_op(ROOTDEV);
  // 同调用create
  if((argstr(0, path, MAXPATH)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEVICE, major, minor)) == 0){
    end_op(ROOTDEV);
    return -1;
  }
  iunlockput(ip);
  end_op(ROOTDEV);
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();
  
  begin_op(ROOTDEV);
  // 很简单：首先拿到path对应的inode，确保它是文件夹，然后将其修改为当前进程的cwd
  if(argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0){
    end_op(ROOTDEV);
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op(ROOTDEV);
    return -1;
  }
  iunlock(ip);
  iput(p->cwd);
  end_op(ROOTDEV);
  p->cwd = ip;
  return 0;
}

uint64
sys_exec(void)
{
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;
  // 获取可执行文件的路径，以及参数数组的❗首地址
  if(argstr(0, path, MAXPATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    // TODO 这里argv的个数实际上是确定的?不就是MAXARG
    if(i >= NELEM(argv)){
      goto bad;
    }
    // uargv是参数数组的首地址，这里将参数一个个取出来放在uarg里
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    // 参数数组是null terminated，最后一个是null，也就是0
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    // 为参数数组里的元素每个分配一个页，然后取到对应的参数字符串
    argv[i] = kalloc();
    if(argv[i] == 0)
      panic("sys_exec kalloc");
    if(fetchstr(uarg, argv[i], PGSIZE) < 0){
      goto bad;
    }
  }
  
  int ret = exec(path, argv);
  // 执行完exec，将刚才每个参数分配到的页free掉
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();
  // 获取pipe数组的地址
  if(argaddr(0, &fdarray) < 0)
    return -1;
  // TODO 
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  // 为上面分配的两个文件，分配fd
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  // 就是设置pipe后得到数组的内容？就是两个fd？这里不能直接赋值吗
  // TODO
  if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}

int 
sys_symlink(void){
  char target[MAXPATH],path[MAXPATH];
  // target,path
  // 在path指定的位置，新建一个链接文件，指向target
  if((argstr(0,target,MAXPATH)<0)||(argstr(1,path,MAXPATH)<0)){
    return -1;
  }
  struct inode *ip;

  begin_op(ROOTDEV);
  // 返回的ip仍然是上锁的，操作完成后应该解锁
  if((ip=(create(path,T_SYMLINK,0,0)))==0){
    end_op(ROOTDEV);
    return -1;
  }
  int l=strlen(target);
  if(l>MAXPATH){
    l=MAXPATH;
  }
  memset(ip->target,0,MAXPATH);
  memmove(ip->target,target,l);
  iunlockput(ip);
  end_op(ROOTDEV);
  return 0;
}