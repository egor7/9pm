#include "sam.h"

static	Block	*blist;

static int
tempdisk(void)
{
	return newtmp(0);
}

Disk*
diskinit()
{
	Disk *d;

	d = emalloc(sizeof(Disk));
	d->fd = tempdisk();
	if(d->fd < 0){
		fprint(2, "sam: can't create temp file: %r\n");
		exits("diskinit");
	}
	return d;
}

static
uint
ntosize(uint n, uint *ip)
{
	uint size;

	if(n > Maxblock)
		panic("internal error: ntosize");
	size = n;
	if(size & (Blockincr-1))
		size += Blockincr - (size & (Blockincr-1));
	/* last bucket holds blocks of exactly Maxblock */
	if(ip)
		*ip = size/Blockincr;
	return size * sizeof(Rune);
}

Block*
disknewblock(Disk *d, uint n)
{
	uint i, j, size;
	Block *b;

	size = ntosize(n, &i);
	b = d->free[i];
	if(b)
		d->free[i] = b->next;
	else{
		/* allocate in chunks to reduce malloc overhead */
		if(blist == nil){
			blist = emalloc(100*sizeof(Block));
			for(j=0; j<100-1; j++)
				blist[j].next = &blist[j+1];
		}
		b = blist;
		blist = b->next;
		b->addr = d->addr;
		d->addr += size;
	}
	b->n = n;
	return b;
}

void
diskrelease(Disk *d, Block *b)
{
	uint i;

	ntosize(b->n, &i);
	b->next = d->free[i];
	d->free[i] = b;
}

void
diskwrite(Disk *d, Block **bp, Rune *r, uint n)
{
	int size, nsize;
	Block *b;

	b = *bp;
	size = ntosize(b->n, nil);
	nsize = ntosize(n, nil);
	if(size != nsize){
		diskrelease(d, b);
		b = disknewblock(d, n);
		*bp = b;
	}
	if(seek(d->fd, b->addr, 0) < 0)
		panic("seek error in temp file");
	if(write(d->fd, r, n*sizeof(Rune)) != n*sizeof(Rune))
		panic("write error to temp file");
	b->n = n;
}

void
diskread(Disk *d, Block *b, Rune *r, uint n)
{
	if(n > b->n)
		panic("internal error: diskread");

	ntosize(b->n, nil);
	if(seek(d->fd, b->addr, 0) < 0)
		panic("seek error in temp file");
	if(read(d->fd, r, n*sizeof(Rune)) != n*sizeof(Rune))
		panic("read error from temp file");
}
