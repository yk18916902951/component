#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct Large
{
    struct Large* next;
    char* alloc;
}Large;

typedef struct Block
{
    char* begin;
    char* end;
    int flag;
    struct Block* next;
}Block;


typedef struct mem_pool
{
    Large* large;
    Block* block;
    int size;
    Block head[0];
}mem_pool;

#define MAX_ALIGN 32
#define MAX_SIZE 4096

mem_pool* create_pool(int size)
{
    char* mem = NULL;
    
    int res = posix_memalign((void**)&mem, MAX_ALIGN, size + sizeof(mem_pool) + sizeof(Block));
    if (res)
        return NULL;
        
    mem_pool* pool = (mem_pool*)mem;
    pool->size = size < MAX_SIZE ? size : MAX_SIZE;
    pool->large = NULL;
    pool->block = (Block*)(mem + sizeof(mem_pool));
    
    
    pool->block->begin = mem + sizeof(mem_pool) + sizeof(Block);
    pool->block->end = pool->block->begin + size;
    pool->block->flag = 0;
    pool->block->next = NULL;
    
    return pool;
}


void mem_destroy(mem_pool* pool)
{
    Large* large = pool->large;
    while(large != NULL)
    {
        Large* next = large->next;
        if (large->alloc != NULL)
            free(large->alloc);
        free(large);     
        large = next;
    }
    
    
    Block* block = pool->block;
    while(block != NULL)
    {
        Block* next = block->next;
        free(block);     
        block = next;
    }
    free(pool);
}


void* alloc_large(mem_pool* pool, int size)
{
    char* mem = NULL;
    int res = posix_memalign((void**)&mem, MAX_ALIGN, size);
    if(res)
        return NULL;
        
    int count = 0;
    for (Large* large = pool->large; large != NULL; large = large->next)
    {
        if(large->alloc == NULL)
        {
            large->alloc = mem;
            return (void*)large->alloc;
        }
        
        if(count++ >= 4)
            break;
    }
  
    Large* newNode = (Large*)malloc(sizeof(Large));
    if(newNode == NULL)
    {
        free(mem);
        return NULL;
    }
    
    newNode->alloc = mem;
    newNode->next = pool->large;
    pool->large = newNode;
    
    return (void*)mem;
}

void* alloc_block(mem_pool* pool, int size)
{
    Block* head = pool->head;
    int alignSize = head->end - (char*)head;
    
    char* mem = NULL;
    int res = posix_memalign((void**)&mem, MAX_ALIGN, alignSize + sizeof(Block));
    if(res)
        return NULL;
    
    Block* block = (Block*)mem;
    block->flag = 1;
    mem += sizeof(Block);
    block->end = mem + alignSize;
    block->begin = mem + size;
    
    block->next = pool->block->next;
    pool->block->next = block;

    return (void*)mem;
}

void* mem_alloc(mem_pool* pool, int size)
{
    if(size < pool->size)
    {
        Block* block = pool->block;
        do 
        {
            if (size <= block->end - block->begin)
            {
                void* mem = block->begin + size;
                block->begin += size;
                ++block->flag;
                return mem;
            }
        } while(block);
        
        return alloc_block(pool, size);
    }
    
    return alloc_large(pool, size);
}

void freee(mem_pool* pool, void* point)
{
    for (Large* large = pool->large; large != NULL; large = large->next)
    {
        if (point == large->alloc)
        {
            free(large->alloc);
            large->alloc = NULL;
            return;
        }
    }
}

int main()
{
    return 0;
}










