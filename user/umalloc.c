#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
  struct
  {
    union header *ptr;
    uint size;
  } s;
  Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

void free(void *ap)
{
  Header *bp, *p;

  bp = (Header *)ap - 1;                                    // header本身不是用户直接使用的，因此将用户传入的内存位置前移一个header的大小，就指向了当前block的meta data
  for (p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr) // 遍历链表，找到自己的位置（按照地址升序组织free list）
    if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))         // p>=p->s.ptr说明p是最后一个（环形链表）
      break;                                                // bp > p || bp < p->s.ptr 说明bp应该作为p的后继
  /**
   *  循环结束有这些可能：
   * 1.找到了合适的位置，p有后继，bp应该放在p与p的后继之间
   * 2.p >= p->s.ptr， p是最右边一个，即遍历中没有找到满足bp > p || bp < p->s.ptr的p，bp应该根据地址大小，作为p的前驱或者后继
   */
  if (bp + bp->s.size == p->s.ptr) // 如果bp的结尾和p的后继的开头相邻，那么他们两个应该合并，修改bp的size，因为bp已经和p的后继合并了，所以bp应该指向p的后继的后继
  {
    bp->s.size += p->s.ptr->s.size;
    bp->s.ptr = p->s.ptr->s.ptr;
  }
  else // 否则，不需要和p的后继合并，此时可能的情况有：1.p有后继，但是不与bp相邻 2.p无后继，所以bp放在p后面（因为是环形链表，所以即使此时bp应该放在开头，bp的后继也是p原来的后继
    bp->s.ptr = p->s.ptr;
  if (p + p->s.size == bp) // p的结尾与bp的开头相邻，那么直接将bp合并进p
  {
    p->s.size += bp->s.size;
    p->s.ptr = bp->s.ptr;
  }
  else // bp不与p合并，说明不相邻，直接让bp作为p的后继
    p->s.ptr = bp;
  freep = p;
}

/**
 * 如果没有free block，那就使用sbrk向os请求更多内存，至少是header的4096倍（为了减少对sbrk的调用）
 * 然后将os分配的内存转型为header *,设置size，调用free将其插入free list，然后返回freep
 */
static Header *
morecore(uint nu)
{
  char *p;
  Header *hp;

  if (nu < 4096)
    nu = 4096;
  p = sbrk(nu * sizeof(Header));
  if (p == (char *)-1)
    return 0;
  hp = (Header *)p;
  hp->s.size = nu;
  free((void *)(hp + 1));
  return freep;
}

void *
malloc(uint nbytes)
{
  Header *p, *prevp;
  uint nunits;

  nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1; // 为了让分配的大小，既包含header，也恰好的是header的整数倍
  if ((prevp = freep) == 0)                                    // 此时没有free block，让头节点指向自己
  {
    base.s.ptr = freep = prevp = &base;
    base.s.size = 0;
  }
  // 注意，即使上面的判断不成立，freep也已经赋值给prevp了
  /**
   * 如果此时没有free block，那么如上，头节点指向自己，size为0，下面就会调用morecore，请求更多内存并插入到free list，因为这是一个循环
   * 链表，所以这里一直向前遍历就可以找到free block，找到之后，如果free block的size刚刚好是要申请的，那么直接从free list中删除
   * 如果大于要申请的，此时将free block的后面（这样就不用修改指针）部分分配出去
   */
  for (p = prevp->s.ptr;; prevp = p, p = p->s.ptr)
  {
    if (p->s.size >= nunits)
    {
      if (p->s.size == nunits)
        prevp->s.ptr = p->s.ptr;
      else
      {
        p->s.size -= nunits;
        p += p->s.size;     // TODO：不明白这里的指针运算为什么没有scale
        p->s.size = nunits; // 设置被分配出去block的size（新header）
      }
      freep = prevp;
      return (void *)(p + 1);
    }
    if (p == freep)
      if ((p = morecore(nunits)) == 0)
        return 0;
  }
}

// int main(){
//   void *p=malloc(10);
//   free(p);
//   malloc(2);
//   return 0;
// }