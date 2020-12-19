/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    ".qum",
    /* First member's full name */
    "paraZite",
    /* First member's email address */
    "md@gmail.com"
};

/* single word (4) or double word (8) alignment */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size)  (((size) + (ALIGNMENT-1)) & ~0x7)

#define MAX(x, y)    ((x) > (y)? (x) : (y))
/* pack a size and alloc bit into header */
#define PACK(size, alloc) ((size) | (alloc))

/* read/write word at address p */
#define GET(p)       (*(size_t *)(p))
#define PUT(p, val)  (*(size_t *)(p) = (val))

/* read size and alloc bit from address p */
#define GET_SIZE(p)  (GET(p) & ~0x1)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* compute address of bp's header/footer */
#define HDRP(bp)     ((void *)(bp) - WSIZE)
#define FTRP(bp)     ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* compute address of bp's next/prev block */
#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((void *)(bp) - GET_SIZE(HDRP(bp) - WSIZE))

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

static char *heap_listp = 0;
static char *free_listp = 0;

/* function prototypes */
void mm_init(void);
void *mm_malloc(size_t size);
void *find_fit(size_t size);
void place(void *bp, size_t size);
void mm_free(void *bp);
void *mm_realloc(void *ptr, size_t size);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                            /* insert padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* epilogue header */
    heap_listp += (2 * WSIZE);

    if(extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    
    /* initialized successfully */
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendedsize;
    char *bp;

    if (size == 0)
        return NULL;

    /* adjust the block's size */
    if (size <= DSIZE)
    {
        asize = 2 * DSIZE;
    }
    else
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    /* find the first free block for requested block (splitting it if needed) */
    if ((bp = find_fit(asize)) != NULL) 
    {
        place(bp, asize);
        return bp;
    }

    /* no fit found, get more memory and place the block (splitting if needed) */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
* find_fit - Finds a free block for a given size
*/
void *find_fit(size_t size)
{
    char *ptr = free_listp;
    size_t block_size;
    /* iterate until hitting epilogue block (which has size 0) */
    while(GET_SIZE(ptr) != 0)
    {
        block_size = GET_SIZE(ptr);
        if(!GET_ALLOC(ptr) && (block_size >= size))
        {
            return (void *)ptr;
        }
        ptr += block_size;
    }
}

/*
* place - Places "size" bytes in bp (splitting if needed)
*/
void place(void *bp, size_t size)
{
    size_t block_size = GET_SIZE(bp);
    PUT(HDRP(bp), PACK(size, 1));

    if(block_size > size)
    {
        size_t split_size = block_size - size;
        void *split_hdr = (void *)((char *)bp + size);
        PUT(FTRP(bp), PACK(split_size, 0));
        PUT(split_hdr, PACK(split_size, 0));
    }
}

/*
 * mm_free - Modifies the block's alloc bit to 0.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/* helper function to extend the heap with given number of words */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    
    size = (words % 2)? (words + 1) * WSIZE : words * WSIZE;

    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));           /* new free block header */
    PUT(FTRP(bp), PACK(size, 0));           /* new free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* epilogue header */

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /* case 1 - both previous and next block are allocated */
    if(prev_alloc && next_alloc) 
    {
        return bp;
    }
    /* case 2 - previous block allocated, next one free */
    else if(prev_alloc && !next_alloc)
    {
        size += GET_SIZE(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    }
    /* case 3 - previous block free, next one allocated */
    else if(!prev_alloc && next_alloc)
    {
        size += GET_SIZE(PREV_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    /* case 4 - both previous and next blocks are free */
    else
    {
        size += GET_SIZE(PREV_BLKP(bp)) + GET_SIZE(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}
