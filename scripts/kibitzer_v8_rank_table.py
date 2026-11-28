#!/usr/bin/env python3
"""Independent reference HU equity ranking. Requires numpy and numba."""
import numpy as np
from numba import njit
@njit
def straight(mask):
    for high in range(12,3,-1):
        if (mask>>(high-4))&31==31:return high+2
    if mask&4111==4111:return 5
    return 0
@njit
def ev(cards):
    cnt=np.zeros(13,np.int8); suits=np.zeros(4,np.int8); sm=np.zeros(4,np.int32);mask=0
    for c in cards:
        r=c%13;s=c//13;cnt[r]+=1;suits[s]+=1;sm[s]|=1<<r;mask|=1<<r
    f=-1
    for s in range(4):
        if suits[s]>=5:f=s
    if f>=0:
        st=straight(sm[f])
        if st:return (8<<20)+(st<<16)
    four=-1;three=-1;second=-1;pair=-1;pair2=-1
    for r in range(12,-1,-1):
        if cnt[r]==4:four=r
        elif cnt[r]==3:
            if three<0:three=r
            else:second=r
        elif cnt[r]==2:
            if pair<0:pair=r
            elif pair2<0:pair2=r
    if four>=0:
        k=12
        while k==four or cnt[k]==0:k-=1
        return (7<<20)+((four+2)<<16)+((k+2)<<12)
    if three>=0 and max(second,pair)>=0:
        return (6<<20)+((three+2)<<16)+((max(second,pair)+2)<<12)
    if f>=0:
        val=5<<20;shift=16
        for r in range(12,-1,-1):
            if sm[f]&(1<<r):
                val+=(r+2)<<shift;shift-=4
                if shift<0:break
        return val
    st=straight(mask)
    if st:return (4<<20)+(st<<16)
    if three>=0:val=(3<<20)+((three+2)<<16);shift=12;skip1=three;skip2=-1;num=2
    elif pair2>=0:val=(2<<20)+((pair+2)<<16)+((pair2+2)<<12);shift=8;skip1=pair;skip2=pair2;num=1
    elif pair>=0:val=(1<<20)+((pair+2)<<16);shift=12;skip1=pair;skip2=-1;num=3
    else:val=0;shift=16;skip1=-1;skip2=-1;num=5
    for r in range(12,-1,-1):
        if cnt[r]>0 and r!=skip1 and r!=skip2:
            val+=(r+2)<<shift;shift-=4;num-=1
            if num==0:break
    return val
@njit
def equity(a,b,n,seed):
    np.random.seed(seed); deck=np.array([i for i in range(52) if i!=a and i!=b])
    h=np.zeros(7,np.int32);v=np.zeros(7,np.int32);h[0]=a;h[1]=b;total=0
    for t in range(n):
        for i in range(7):
            j=np.random.randint(i,50);deck[i],deck[j]=deck[j],deck[i]
        v[0]=deck[0];v[1]=deck[1]
        for i in range(5):h[i+2]=deck[i+2];v[i+2]=deck[i+2]
        x=ev(h);y=ev(v);total+=2*(x>y)+(x==y)
    return total/(2*n)
if __name__=='__main__':
    n=100000
    for i in range(13):
        print('{'+','.join(str(round(10000*equity(max(i,j),min(i,j)+(0 if i<j else 13),n,20261128+13*i+j))) for j in range(13))+'},')
