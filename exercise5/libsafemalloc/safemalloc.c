#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "safemalloc.h"

#define		SAFEMALLOC_ARENA		((void *)0x40402000)
#define		SAFEMALLOC_ARENA_SIZE		(8UL << 20)

#define		DEBUG_FREE_LIST	1
#define		DEBUG_FULL_LIST	2
#define		DEBUG_COALESCE	4
#define		DEBUG_POISON	8
#define		DEBUG_RETURNS	16
#define		DEBUG_EXCESSIVE	32

#define		CHUNK_FREE	(1UL)
#define		CHUNK_TOP	(2UL)
#define		CHUNK_FLAGBITS	(CHUNK_FREE | CHUNK_TOP)

#define		WORD_SIZE	(sizeof(void *))
#define		WORD_MASK	(WORD_SIZE - 1)

/**
 * SafeMalloc -- implementation details
 *
 * (1) Chunk structure: occupied chunks
 * +------------------------------------------------------------+
 * |   Size of this chunk (excluding header), chunk flags       |
 * +------------------------------------------------------------+
 * |   Size of previous chunk (excluding header)                |
 * +------------------------------------------------------------+
 * |   USER DATA                                                |
 * |   ...                                                      |
 * +------------------------------------------------------------+
 *
 * (2) Chunk structure: free chunks
 * +------------------------------------------------------------+
 * |   Size of this chunk (excluding header), chunk flags       |
 * +------------------------------------------------------------+
 * |   Size of previous chunk (excluding header)                |
 * +------------------------------------------------------------+
 * |   Pointer to next free chunk (linked circular list)        |
 * +------------------------------------------------------------+
 * |   FREE SPACE                                               |
 * |   ...                                                      |
 * +------------------------------------------------------------+
 *
 * (3) Chunk sizes are always calculated using the "short" header,
 * i.e. as if the chunk was in use. This allows turning a free
 * chunk into an occupied one by just flipping the CHUNK_FREE
 * bit (and removing it from the singly-linked free list).
 *
 * (4) On application startup the heap is created as a single
 * mmap()ped memory area of size SAFEMALLOC_ARENA_SIZE (a compile-time
 * constant). It cannot be expanded.
 * Initially the whole area is registered as a single free chunk.
 *
 * (5) SafeMalloc includes an extensive debug facility. The environ-
 * ment variable "SAFEMALLOC_DEBUG" can be set to a bitwise OR of
 * the following constants to activate individual subgroups:
 * - 1		print the free list on every xmalloc() and xfree()
 * - 2		print the complete chunk list on every xmalloc()/xfree()
 *   (these two are mutually exclusive, "free list" takes precedence)
 * - 4		report every merge of chunks
 * - 8		initialize memory with marker bytes:
 *   - 0xaa	xmalloc()'ed data area
 *   - 0x55	xfree()'d space
 *   - 0x56	header structure overwritten during a forward merge
 *   - 0x57	header structure overwritten during a backward merge
 * - 16		report invocations (similar to ltrace)
 * - 32		also show data/space contents while traversing the list
 **/

/* header structure, see above for an explanation */
union chunk {
  struct free_chunk {
    unsigned long size, prev_size;
    union chunk *next_free;
    char data[0];
  } free;
  struct used_chunk {
    unsigned long size, prev_size;
    char data[0];
  } used;
};

/* base pointer to the mmap()ped heap */
static void *xmalloc_arena = NULL;
/* debug facility bits */
static int xmalloc_debug = 0;
/* head (and tail) of the free list */
static union chunk *free_head = NULL;

static int out_of_heap_bounds(void *heap_ptr) {
	if (heap_ptr < SAFEMALLOC_ARENA)
		return 1;
	if (heap_ptr >= SAFEMALLOC_ARENA + SAFEMALLOC_ARENA_SIZE)
		return 1;

	return 0;
}

void xdbg_set(int ll) {
  xmalloc_debug = ll;
  return;
}

int xdbg_get() {
  return xmalloc_debug;
}

void xdbg(void) {
	union chunk *F;

    if (xmalloc_debug & DEBUG_FREE_LIST) {

	fprintf(stderr, "FREE LIST DEBUG\n");
	F = free_head;
	do {
		unsigned int *Xs, *Xe, *X;

		if (out_of_heap_bounds(F)) {
			fprintf(stderr, "Chunk @%10p (heap broken?!?)\n", F);
			break;
		}

		Xs = (unsigned int *)F;
		Xe = (unsigned int *)(F->used.data + (F->free.size & ~CHUNK_FLAGBITS));

		fprintf(stderr, "Chunk @%10p (size %8lx) (prev size %8lx) (next @%10p)\n", \
			F, F->free.size, F->free.prev_size, F->free.next_free);

		if (xmalloc_debug & DEBUG_EXCESSIVE) {
			for (X = Xs; X < Xe && (X - Xs) < 20; X++)
				fprintf(stderr, "%08x ", *X);
			fprintf(stderr, "\n");
		}

		F = F->free.next_free;
	} while (F != free_head);

    } else if (xmalloc_debug & DEBUG_FULL_LIST) {

	fprintf(stderr, "CHUNK DEBUG (free head = %p)\n", free_head);
	F = xmalloc_arena;
	do {
		if (out_of_heap_bounds(F)) {
			fprintf(stderr, "Chunk @%10p (heap broken?!?)\n", F);
			break;
		}

		if (F->used.size & CHUNK_FREE)
			fprintf(stderr, "Chunk @%10p (size %8lx) (prev size %8lx) (next @%10p)\n", \
				F, F->free.size, F->free.prev_size, F->free.next_free);
		else
			fprintf(stderr, "Chunk @%10p (size %8lx) (prev size %8lx)\n", \
				F, F->free.size, F->free.prev_size);

		if ((F->used.size & CHUNK_FREE) && F->free.next_free == free_head) break;

		F = (union chunk *)((char *)F + sizeof(struct used_chunk) + (F->used.size & ~CHUNK_FLAGBITS));
	} while (1);

    }
}

void *xmalloc(unsigned int size) {
	union chunk *F = free_head, *Fprev = NULL;

	// round up allocation request to word size
	if (size & WORD_MASK) {
		size = (size + WORD_SIZE) & ~WORD_MASK;
	}

	// allocate at least enough memory to free the chunk later
	if (size < (sizeof(struct free_chunk) - sizeof(struct used_chunk))) {
		size = (sizeof(struct free_chunk) - sizeof(struct used_chunk));
	}

	// search free list for the first chunk of sufficient size
	do {
		if ((F->free.size & ~CHUNK_FLAGBITS) >= size) break;
		Fprev = F;
		F = F->free.next_free;
	} while (F != free_head || (F = NULL));

	// if there is no such chunk, bad luck
	if (!F) return NULL;

	// if Fprev is still unset, the first chunk must have matched our allocation request;
	//   traverse list completely to find Fprev (it is cyclic)
	if (!Fprev) {
		for (Fprev = free_head; Fprev->free.next_free != F; Fprev = Fprev->free.next_free) ;
	}

	// if Fprev cannot be found, list is probably broken, so deny allocation
	if (!Fprev) return NULL;

	// Preparation complete. We have F, size (word-adjusted) and Fprev.

	// Case 1: there's enough room for splitting the chunk (and retaining a free "rest")
	if ((F->free.size & ~CHUNK_FLAGBITS) > size + sizeof(struct used_chunk)) {
		union chunk *G = (union chunk *)(F->used.data + size);
		union chunk *H;
		if (F->free.next_free == F) {
			G->free.next_free = G;
		} else {
			G->free.next_free = F->free.next_free;
		}
		G->free.size = F->free.size - sizeof(struct used_chunk) - size;
		G->free.prev_size = size;
		Fprev->free.next_free = G;
		if (F == free_head) free_head = G;
		if (!(G->free.size & CHUNK_TOP)) {
			H = (union chunk *)(G->used.data + (G->free.size & ~CHUNK_FLAGBITS));
			H->free.prev_size = G->free.size & ~CHUNK_FLAGBITS;
		}
		/* from here on, F is used */
		F->used.size = size;

	// Case 2: the list contains only one chunk, and after allocation it would be gone:
	//   deny allocation in this case (would require too much special handling)
	} else if (Fprev == F) {
		return NULL;

	// Case 3: chunk is barely large enough; hand it out completely
	} else {
		Fprev->free.next_free = F->free.next_free;
		if (F == free_head) free_head = F->free.next_free;
		/* from here on, F is used */
		F->used.size &= ~CHUNK_FLAGBITS;
		size = F->used.size;
	}

	// poison data area
	if (xmalloc_debug & DEBUG_POISON) memset(F->used.data, 0xaa, size);

	// generate debug output if desired
	xdbg();
	if (xmalloc_debug & DEBUG_RETURNS)
		fprintf(stderr, "xmalloc(%d) => %p\n", size, &F->used.data);

	return (void *)(&F->used.data);
}

void xfree(void *ptr) {
	union chunk *F = (union chunk *)((struct used_chunk *)ptr - 1);
	union chunk *Fprev = free_head;
	union chunk *G;

	// if pointer is outside our arena, reject
	// NB: void pointer arithmetics are allowed nowadays? Uaaah...
	if ((ptr < SAFEMALLOC_ARENA) || (ptr >= SAFEMALLOC_ARENA + SAFEMALLOC_ARENA_SIZE))
		return;

	// no double frees - just return
	if (F->used.size & CHUNK_FREE)
		return;

	// poison freed area, mark chunk as free
	if (xmalloc_debug & DEBUG_POISON) memset(F->used.data, 0x55, F->used.size);
	F->used.size |= CHUNK_FREE;

	// insert chunk into free list; search for the right place (i.e. keep it sorted)
	do {
		// Fprev->next points back, so Fprev is the last chunk
		// => ours is going to be the very first or last
		// => either way, it's a circular list, so really it's the same
		if (Fprev->free.next_free <= Fprev) break;

		// Fprev < F < Fprev->next => found our insertion slot
		if (Fprev < F && Fprev->free.next_free > F) break;

		Fprev = Fprev->free.next_free;
	} while (Fprev != free_head || (Fprev = NULL));

	// technically, this cannot happen
	if (!Fprev) *(char *)0 = 0;

	// perform the insertion
	F->free.next_free = Fprev->free.next_free;
	Fprev->free.next_free = F;

	// if the head has changed, update it
	if (F < free_head) free_head = F;

	// the free list is in good shape again now - what follows is optimization:
	// Consolidation of adjacent free chunks

	// A) consolidate forwards (F integrates F->next)
	if (!(F->free.size & CHUNK_TOP)) {
		G = (union chunk *)(F->used.data + (F->free.size & ~CHUNK_FLAGBITS));
		if (G->used.size & CHUNK_FREE) {
			// prerequisites ok: F is not the top chunk && the one immediately next to F is free

			if (xmalloc_debug & DEBUG_COALESCE) fprintf(stderr, "Coalescing FD @%10p adding @%10p\n", F, G);

			// merge G into F
			F->free.size += sizeof(struct used_chunk) + (G->free.size & ~CHUNK_FLAGBITS);
			F->free.size |= (G->free.size & CHUNK_TOP);
			F->free.next_free = G->free.next_free;

			if (xmalloc_debug & DEBUG_POISON) memset(G, 0x56, sizeof(struct free_chunk));

			// if F is still not the top chunk, update our new neighbour's prev_size
			if (!(F->free.size & CHUNK_TOP)) {
				G = (union chunk *)(F->used.data + (F->free.size & ~CHUNK_FLAGBITS));
fprintf(stderr, "Write due to forward consolidation at %p\n", &(G->used.prev_size));
				G->used.prev_size = F->free.size & ~CHUNK_FLAGBITS;
			}

		}
	}

	// B) consolidate backwards (F is integrated into its predecessor)
	if (F->free.prev_size != ~0UL) {
		G = (union chunk *)((char *)F - F->free.prev_size - sizeof(struct used_chunk));
		if (G->used.size & CHUNK_FREE) {
			// prerequisites ok: F is not the head && the one immediately before F is free

			if (xmalloc_debug & DEBUG_COALESCE) fprintf(stderr, "Coalescing BK @%10p into @%10p\n", F, G);

			// merge F into G
			G->free.size += sizeof(struct used_chunk) + (F->free.size & ~CHUNK_FLAGBITS);
			G->free.size |= (F->free.size & CHUNK_TOP);
			G->free.next_free = F->free.next_free;

			if (xmalloc_debug & DEBUG_POISON) memset(F, 0x57, sizeof(struct free_chunk));

			// if the updated chunk has not become the top chunk, update F->next's prev_size
			F = G;
			if (!(F->free.size & CHUNK_TOP)) {
				G = (union chunk *)(F->used.data + (F->free.size & ~CHUNK_FLAGBITS));
fprintf(stderr, "Write due to backward consolidation at %p\n", &(G->used.prev_size));
				G->used.prev_size = F->free.size & ~CHUNK_FLAGBITS;
			}
		}
	}

	// generate debug output if desired
	xdbg();
	if (xmalloc_debug & DEBUG_RETURNS) fprintf(stderr, "xfree(%p)\n", ptr);
}

void __attribute__((constructor)) xmalloc_setup(void) {
	// allocate memory, die if allocation fails
	xmalloc_arena = mmap(SAFEMALLOC_ARENA, SAFEMALLOC_ARENA_SIZE, \
			PROT_READ | PROT_WRITE, \
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, \
			-1, 0);
	if (xmalloc_arena == MAP_FAILED) {
		*(long *)0 = 0;
	}

	// create the initial empty chunk, initialize free list
	free_head = xmalloc_arena;
	free_head->free.size = (SAFEMALLOC_ARENA_SIZE - sizeof(struct used_chunk)) | CHUNK_FREE | CHUNK_TOP;
	free_head->free.prev_size = ~0UL;
	free_head->free.next_free = free_head;

	xdbg();
}
