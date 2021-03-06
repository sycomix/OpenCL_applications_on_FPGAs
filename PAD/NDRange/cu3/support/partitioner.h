#ifndef _PARTITIONER_H_
#define _PARTITIONER_H_

#ifndef _OPENCL_COMPILER_
#include <iostream>
#endif

#if !defined(_OPENCL_COMPILER_) && defined(OCL_2_0)
#include <atomic>
#endif

// Partitioner definition -----------------------------------------------------

typedef struct Partitioner {

    int n_tasks;
    int cut;
    int current;
#ifndef _OPENCL_COMPILER_
    int thread_id;
    int n_threads;
#endif


#ifdef OCL_2_0
    // OpenCL 2.0 support for dynamic partitioning
    int strategy;
#ifdef _OPENCL_COMPILER_
    __global atomic_int *worklist;
    __local int *tmp;
#else
    std::atomic_int *worklist;
#endif
#endif

} Partitioner;

// Partitioning strategies
#define STATIC_PARTITIONING 0
#define DYNAMIC_PARTITIONING 1

// Create a partitioner -------------------------------------------------------

inline Partitioner partitioner_create(int n_tasks, float alpha
#ifndef _OPENCL_COMPILER_
    , int thread_id, int n_threads
#endif
#ifdef OCL_2_0
#ifdef _OPENCL_COMPILER_
    , __global atomic_int *worklist
    , __local int *tmp
#else
    , std::atomic_int *worklist
#endif
#endif
    ) {
    Partitioner p;
    p.n_tasks = n_tasks;
#ifndef _OPENCL_COMPILER_
    p.thread_id = thread_id;
    p.n_threads = n_threads;
#endif
    if(alpha >= 0.0 && alpha <= 1.0) {
        p.cut = p.n_tasks * alpha;
#ifdef OCL_2_0
        p.strategy = STATIC_PARTITIONING;
#endif
    } else {
#ifdef OCL_2_0
        p.strategy = DYNAMIC_PARTITIONING;
        p.worklist = worklist;
#ifdef _OPENCL_COMPILER_
        p.tmp = tmp;
#endif
#endif
    }
    return p;
}

// Partitioner iterators: first() ---------------------------------------------

#ifndef _OPENCL_COMPILER_

inline int cpu_first(Partitioner *p) {
#ifdef OCL_2_0
    if(p->strategy == DYNAMIC_PARTITIONING) {
        p->current = p->worklist->fetch_add(1);
    } else
#endif
    {
        p->current = p->thread_id;
    }
    return p->current;
}

#else

inline int gpu_first(Partitioner *p) {
#ifdef OCL_2_0
    if(p->strategy == DYNAMIC_PARTITIONING) {
        if(get_local_id(1) == 0 && get_local_id(0) == 0) {
            p->tmp[0] = atomic_fetch_add(p->worklist, 1);
        }
        barrier(CLK_LOCAL_MEM_FENCE);
        p->current = p->tmp[0];
    } else
#endif
    {
        p->current = p->cut + get_group_id(0);
    }
    return p->current;
}

#endif

// Partitioner iterators: more() ----------------------------------------------

#ifndef _OPENCL_COMPILER_

inline bool cpu_more(const Partitioner *p) {
#ifdef OCL_2_0
    if(p->strategy == DYNAMIC_PARTITIONING) {
        return (p->current < p->n_tasks);
    } else
#endif
    {
        return (p->current < p->cut);
    }
}

#else

inline bool gpu_more(const Partitioner *p) {
    return (p->current < p->n_tasks);
}

#endif

// Partitioner iterators: next() ----------------------------------------------

#ifndef _OPENCL_COMPILER_

inline int cpu_next(Partitioner *p) {
#ifdef OCL_2_0
    if(p->strategy == DYNAMIC_PARTITIONING) {
        p->current = p->worklist->fetch_add(1);
    } else
#endif
    {
        p->current = p->current + p->n_threads;
    }
    return p->current;
}

#else

inline int gpu_next(Partitioner *p) {
#ifdef OCL_2_0
    if(p->strategy == DYNAMIC_PARTITIONING) {
        if(get_local_id(1) == 0 && get_local_id(0) == 0) {
            p->tmp[0] = atomic_fetch_add(p->worklist, 1);
        }
        barrier(CLK_LOCAL_MEM_FENCE);
        p->current = p->tmp[0];
    } else
#endif
    {
        p->current = p->current + get_num_groups(0);
    }
    return p->current;
}

#endif

#endif

