#include <sys/memory.h>

#include "include/mem.h"
#include "include/vsh_exports.h"


static sys_memory_container_t mc_app = (sys_memory_container_t)-1;
static sys_addr_t heap_mem = 0;
static uint32_t heap_size = 0;
static uint32_t prx_heap = 0;

/***********************************************************************
*
***********************************************************************/
void destroy_heap(void)
{
	if(heap_mem) sys_memory_free((sys_addr_t)heap_mem);
	heap_mem = heap_size = prx_heap = 0;
	mc_app = (sys_memory_container_t)-1;
}

/***********************************************************************
* create prx heap from memory container 1("app")
***********************************************************************/
int32_t create_heap(int32_t size)
{
	mc_app = vsh_memory_container_by_id(1);
	if (!mc_app) {destroy_heap(); return(-1);}

	heap_size = MB(size);

	sys_memory_allocate_from_container(heap_size, mc_app, SYS_MEMORY_PAGE_SIZE_1M, &heap_mem);
	if (!heap_mem) {destroy_heap(); return(-1);}

	prx_heap = (uint32_t)heap_mem;
	return(0);
}

/***********************************************************************
*
***********************************************************************/
void *mem_alloc(uint32_t size)
{
	uint32_t add = prx_heap;
	if(((prx_heap-heap_mem)+size)<heap_size) {prx_heap += size; char *mem = (char*)prx_heap; *mem = 0;}

	return (void*)add;
}

/***********************************************************************
*
***********************************************************************/
int32_t mem_free(uint32_t size)
{
	if(prx_heap>=size) prx_heap -= size; else return (-1);
	return(0);
}

/***********************************************************************
*
***********************************************************************/
void memcpy32(void *dst, const void *src, size_t n)
{
	uint32_t *d = (uint32_t *) dst;
	uint32_t *s = (uint32_t *) src;
	while (n--) *d++ = *s++;
}
