/*
 * Simple, 32-bit and 64-bit clean allocator based on implicit free
* lists, first fit placement, and boundary tag coalescing, as described
 * in the CS:APP2e text. Blocks must be aligned to doubleword (8 byte)
 * boundaries. Minimum block size is 16 bytes.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mm.h"
#include "memlib.h"

/*
 * If NEXT_FIT defined use next fit search, else use first fit search
 */
#define NEXT_FITx

#define ALIGNMENT 8
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT - 1)) & ~0x7)
/* $begin mallocmacros */
/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
#define DSIZE       8       /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            //line:vm:mm:get
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    //line:vm:mm:put

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   //line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
/* $end mallocmacros */

/*macros written by me*/
#define GLONG(p) (*(unsigned long *)(p))
#define PLONG(p, val) (*(unsigned long *)(p) = (val))
#define NEXT_FREE(bp) ((char *)GLONG(bp))
#define PREV_FREE(bp) ((char *)GLONG((char *)bp + DSIZE))
#define SET_NEXT(bp, val) PLONG((char *)bp, (long)val)
#define SET_PREV(bp, val) PLONG((char *)bp + DSIZE, (long)val)
/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */
static char *free_list;
int debug = 1;
/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void checkheap(int verbose);
static void checkblock(void *bp);
static void add_node(void *bp);
static void delete_node(void *bp);
/*
 * mm_init - Initialize the memory manager
 */
/* $begin mminit */
int mm_init(void)
{
    if(debug == 1) printf("init running \n");
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) //line:vm:mm:begininit
        return -1;
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2*WSIZE);                     //line:vm:mm:endinit

    free_list = NULL;

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}

/*
 * mm_malloc - Allocate a block with at least size bytes of payload
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size)
{
    if(debug == 1) printf("malloc running \n");
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;
    void *temp;
    if(debug == 1) printf("Starting malloc\n");
    if(debug == 1)
    {
        for(temp = free_list; temp != NULL;
            temp = NEXT_FREE(temp))
          {
            printf("NODE: %p\n", temp);
            if(PREV_FREE(temp) != NULL)
              printf("Prev %p\n", PREV_FREE(temp));
            if(NEXT_FREE(temp) != NULL)
              printf("Next %p\n", NEXT_FREE(temp));
          }
    }


/* $end mmmalloc */
    if (heap_listp == 0){
        mm_init();
    }
/* $begin mmmalloc */
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
            asize = 3*DSIZE;
    else
            asize = ALIGN(size +DSIZE);//DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    if(debug == 1) printf("malloc, right before find_fit\n");
    if(debug == 1)
    {
        for(temp = free_list; temp != NULL;
            temp = NEXT_FREE(temp))
          {
            printf("NODE: %p\n", temp);
            if(PREV_FREE(temp) != NULL)
              printf("Prev %p\n", PREV_FREE(temp));
            if(NEXT_FREE(temp) != NULL)
              printf("Next %p\n", NEXT_FREE(temp));
          }
    }


    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {  //line:vm:mm:findfitcall
            place(bp, asize);                  //line:vm:mm:findfitplace
            return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);                 //line:vm:mm:growheap1
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
            return NULL;                                  //line:vm:mm:growheap2
    place(bp, asize);                                 //line:vm:mm:growheap3
    return bp;
}
/* $end mmmalloc */

/*
 * mm_free - Free a block
 */
/* $begin mmfree */
void mm_free(void *bp)
{
    if(debug == 1) printf("free running \n");
    if(bp == 0)
        return;

    size_t size = GET_SIZE(HDRP(bp));
    if (heap_listp == 0){
        mm_init();
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
    add_node(bp);
}

/* $end mmfree */
/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
/* $begin mmfree */
static void *coalesce(void *bp)
{
  if(debug == 1)
  { 
      printf("coalesce running \n");
    void* temp;
    for(temp = free_list; temp != NULL;
        temp = NEXT_FREE(temp))
      {
        printf("NODE: %p\n", temp);
        if(PREV_FREE(temp) != NULL)
          printf("Prev %p\n", PREV_FREE(temp));
        if(NEXT_FREE(temp) != NULL)
          printf("Next %p\n", NEXT_FREE(temp));
      }

    printf("BP IS %p\n", bp);
    printf("BP PREV IS %p\n", PREV_BLKP(bp));
    printf("HDRP BP is %p\n", HDRP(bp));
    printf("get size of hdrp bp %du\n", GET_SIZE(HDRP(bp)));
    //printf("BP PREV FOOTER IS %p\n", FTRP(PREV_BLKP(bp)));

   }
  size_t prev_alloc;
  
  if((unsigned long)(PREV_BLKP(bp)) <= (unsigned long)heap_listp) prev_alloc = 1;
  else prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
  
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            /* Case 1 */
        return bp;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        delete_node(PREV_BLKP(bp));
        bp = PREV_BLKP(bp);
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else {                                     /* Case 4 */
        delete_node(PREV_BLKP(bp));
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(HDRP(bp), PACK(size, 0));
    }
/* $end mmfree
    if ((rover > (char *)bp) && (rover < NEXT_BLKP(bp)))
        rover = bp;*/
    return bp;
}
/* $end mmfree */

/*
 * mm_realloc - Naive implementation of realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    if(debug == 1) printf("realloc running\n");
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}

/*
 * checkheap - We don't check anything right now.
 */
void mm_checkheap(int verbose)
{
    verbose = verbose;
}

/*
 * The remaining routines are internal helper routines
 */

static void add_node(void *bp)
{
    void *temp;
    if(debug == 1) printf("\n");
    if(debug == 1) printf("Starting add_node, bp = %p\n", bp);
    if(debug == 1)
    {
        for(temp = free_list; temp != NULL; temp = NEXT_FREE(temp))
          {
            printf("NODE: %p\n", temp);
            if(PREV_FREE(temp) != NULL)
              printf("Prev %p\n", PREV_FREE(temp));
            if(NEXT_FREE(temp) != NULL)
              printf("Next %p\n", NEXT_FREE(temp));
          }
    }
    if(free_list != NULL)
    {
        SET_PREV(bp, NULL);
        SET_PREV(free_list, bp);
        SET_NEXT(bp, free_list);
        free_list = bp;
    }
    else
    {
        free_list = bp;
        SET_PREV(bp, NULL);
        SET_NEXT(bp, NULL);
    }
    if(debug == 1)
      {
        printf("Ending add_node, bp = %p\n", bp);

        printf("New node %p's next is %p\n", bp, NEXT_FREE(bp));
        for(temp = free_list; temp != NULL;
            temp = NEXT_FREE(temp))
          {
            printf("NODE: %p\n", temp);
            if(PREV_FREE(temp) != NULL)
              printf("Prev %p\n", PREV_FREE(temp));
            if(NEXT_FREE(temp) != NULL)
              printf("Next %p\n", NEXT_FREE(temp));
          }
    }
}
static void delete_node(void *bp)
{
    if(debug == 1) printf("delete node running \n");
    
    void *prev_free = PREV_FREE(bp);
    void *next_free = NEXT_FREE(bp);
    
    if(debug == 1) printf("\n");
    void* temp;
    if(debug == 1) printf("Starting del_node, bp = %p\n", bp);
    if(debug == 1)
    {
        for(temp = free_list; temp != NULL;
            temp = NEXT_FREE(temp))
          {
            printf("NODE: %p\n", temp);
            if(PREV_FREE(temp) != NULL)
              printf("Prev %p\n", PREV_FREE(temp));
            if(NEXT_FREE(temp) != NULL)
              printf("Next %p\n", NEXT_FREE(temp));
          }
    }

    if(prev_free == NULL && next_free == NULL)
    {
        free_list = NULL;
    }
    else if(prev_free != NULL && next_free == NULL)
    {
        SET_NEXT(prev_free, NULL);
    }
    else if(prev_free == NULL && next_free != NULL)
    {
        SET_PREV(next_free, NULL);
        free_list = next_free;
    }
    else
    {
        SET_PREV(next_free, prev_free);
        SET_NEXT(prev_free, next_free);
    }
}
/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words)
{
    if(debug == 1) printf("extend heap running \n");
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; //line:vm:mm:beginextend
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;                                        //line:vm:mm:endextend

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   //line:vm:mm:freeblockhdr
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   //line:vm:mm:freeblockftr
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ //line:vm:mm:newepihdr
    /* Coalesce if the previous block was free */
    //add_node(bp);
    coalesce(bp);                                          //line:vm:mm:returnblock
    add_node(bp);
    return bp;
}
/* $end mmextendheap */

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(void *bp, size_t asize)
     /* $end mmplace-proto */
{
    if(debug == 1) printf("place running \n");
    void *temp;
    if(debug == 1) printf("Starting place, bp = %p\n", bp);
    if(debug == 1)
    {
        for(temp = free_list; temp != NULL;
            temp = NEXT_FREE(temp))
          {
            printf("NODE: %p\n", temp);
            if(PREV_FREE(temp) != NULL)
              printf("Prev %p\n", PREV_FREE(temp));
            if(NEXT_FREE(temp) != NULL)
              printf("Next %p\n", NEXT_FREE(temp));
          }
    }

    size_t csize = GET_SIZE(HDRP(bp));
    //split
    if ((csize - asize) >= (3*DSIZE)) {
        
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        delete_node(bp);
        
        bp = NEXT_BLKP(bp);
        
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        add_node(bp);
    }
    //no split
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        delete_node(bp);
    }


    if(debug == 1) printf("Ending place, bp = %p\n", bp);
    if(debug == 1)
    {
        for(temp = free_list; temp != NULL;
            temp = NEXT_FREE(temp))
          {
            printf("NODE: %p\n", temp);
            if(PREV_FREE(temp) != NULL)
              printf("Prev %p\n", PREV_FREE(temp));
            if(NEXT_FREE(temp) != NULL)
              printf("Next %p\n", NEXT_FREE(temp));
      }
    }



}
/* $end mmplace */

/*
 * find_fit - Find a fit for a block with asize bytes
 */
/* $begin mmfirstfit */
/* $begin mmfirstfit-proto */
static void *find_fit(size_t asize)
{
   if(debug == 1)  printf("find fit running \n");
    void *temp;
    void *fit = NULL;
    size_t counter = 0;
    size_t diff;
    size_t min_diff = ~0;
    if(debug == 1) printf("Starting find fit\n");

    for(temp = free_list; (temp != NULL) && GET_SIZE(HDRP(temp)) > 0; temp = NEXT_FREE(temp))
      {
        if(debug == 1)
          printf("FF%p\n", temp);
        if(asize <= GET_SIZE(HDRP(temp)))
          {
            diff = GET_SIZE(HDRP(temp)) - asize;
            if(diff < min_diff)
              {
                fit = temp;
              }
            counter++;
            if(counter == 20)
              {
                return fit;
              }
          }
      }

    return fit;
}

static void printblock(void *bp)
{
    size_t hsize, halloc, fsize, falloc;

    checkheap(0);
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }

    /*  printf("%p: header: [%p:%c] footer: [%p:%c]\n", bp,
        hsize, (halloc ? 'a' : 'f'),
        fsize, (falloc ? 'a' : 'f')); */
}

static void checkblock(void *bp)
{
    if ((size_t)bp % 8)
        printf("Error: %p is not doubleword aligned\n", bp);
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
        printf("Error: header does not match footer\n");
}

/*
 * checkheap - Minimal check of the heap for consistency
 */
void checkheap(int verbose)
{
    char *bp = heap_listp;

    if (verbose)
        printf("Heap (%p):\n", heap_listp);

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("Bad prologue header\n");
    checkblock(heap_listp);

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (verbose)
            printblock(bp);
        checkblock(bp);
    }

    if (verbose)
        printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");
}

