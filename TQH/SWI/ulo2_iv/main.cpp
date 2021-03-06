#include "kernel.h"
#include "support/common.h"
#include "support/ocl.h"
#include "support/timer.h"
#include "support/verify.h"

#include <string.h>
#include <unistd.h>
#include <thread>
#include <assert.h>

#define AOCL_ALIGNMENT 64
#include <malloc.h>


// Params ---------------------------------------------------------------------
struct Params {

    int         platform;
    int         device;
    int         n_threads;
    int         n_warmup;
    int         n_reps;
    const char *file_name;
    int         pool_size;
    int         queue_size;
    int         m;
    int         n;
    int         n_bins;

    Params(int argc, char **argv) {
        platform      = 0;
        device        = 0;
        n_threads     = 1;
        n_warmup      = 1;
        n_reps        = 10;
        file_name     = "input/basket/basket";
        pool_size     = 1600;
        queue_size    = 320;
        m             = 288;	//height
        n             = 352;	//width
        n_bins        = 256;        
    }
};

// Input Data -----------------------------------------------------------------
void read_input(int *data, task_t *task_pool, const Params &p) {
    // Read frames from files
    int   fr = 0;
    char  dctFileName[100];
    FILE *File;
    float v;
    int   frame_size = p.n * p.m;
    for(int i = 0; i < p.pool_size; i++) {
        sprintf(dctFileName, "%s%d.float", p.file_name, (i % 2));
        if((File = fopen(dctFileName, "rt")) != NULL) {
            for(int y = 0; y < p.m; ++y) {
                for(int x = 0; x < p.n; ++x) {
                    fscanf(File, "%f ", &v);
                    *(data + i * frame_size + y * p.n + x) = (int)v;
                }
            }
            fclose(File);
        } else {
            printf("Unable to open file %s\n", dctFileName);
            exit(-1);
        }
        fr++;
    }

    for(int i = 0; i < p.pool_size; i++) {
        task_pool[i].id = i;
        task_pool[i].op = SIGNAL_WORK_KERNEL;
    }
}

// Main ------------------------------------------------------------------------------------------
int main(int argc, char **argv) {
    const Params p(argc, argv);
    OpenCLSetup  ocl(p.platform, p.device);
    Timer        timer;
    cl_int       clStatus;

    // Allocate
    timer.start("Allocation");
    int     frame_size    = p.n * p.m;
	task_t *h_task_pool   = (task_t *)_aligned_malloc(p.pool_size * sizeof(task_t),AOCL_ALIGNMENT);
	task_t *h_task_queues = (task_t *)_aligned_malloc(p.queue_size * sizeof(task_t),AOCL_ALIGNMENT);
    cl_mem  d_task_queues = clCreateBuffer(ocl.clContext, CL_MEM_READ_WRITE, p.queue_size * sizeof(task_t), NULL, &clStatus);
	int *   h_data_pool   = (int *)_aligned_malloc(p.pool_size * frame_size * sizeof(int),AOCL_ALIGNMENT);
	int *   h_data_queues = (int *)_aligned_malloc(p.queue_size * frame_size * sizeof(int),AOCL_ALIGNMENT);
    cl_mem  d_data_queues =
        clCreateBuffer(ocl.clContext, CL_MEM_READ_WRITE, p.queue_size * frame_size * sizeof(int), NULL, &clStatus);
	int *  h_histo        = (int *)_aligned_malloc(p.pool_size * p.n_bins * sizeof(int),AOCL_ALIGNMENT);
	int *  h_histo_queues = (int *)_aligned_malloc(p.queue_size * p.n_bins * sizeof(int),AOCL_ALIGNMENT);
    cl_mem d_histo_queues =
        clCreateBuffer(ocl.clContext, CL_MEM_READ_WRITE, p.queue_size * p.n_bins * sizeof(int), NULL, &clStatus);
    int *  h_consumed = (int *)malloc(sizeof(int));
    cl_mem d_consumed = clCreateBuffer(ocl.clContext, CL_MEM_READ_WRITE, sizeof(int), NULL, &clStatus);
    clFinish(ocl.clCommandQueue);
    CL_ERR();
    ALLOC_ERR(h_task_pool, h_task_queues, h_data_pool, h_data_queues, h_histo, h_histo_queues, h_consumed);
    timer.stop("Allocation");
    timer.print("Allocation", 1);

    // Initialize
    timer.start("Initialization");
    const int max_wi = ocl.max_work_items(ocl.clKernel);
    read_input(h_data_pool, h_task_pool, p);
    memset((void *)h_histo_queues, 0, p.queue_size * p.n_bins * sizeof(int));
    memset((void *)h_consumed, 0, sizeof(int));
    timer.stop("Initialization");
    timer.print("Initialization", 1);

    for(int rep = 0; rep < p.n_reps + p.n_warmup; rep++) {

        // Reset
        int n_written_tasks = 0;

        for(int n_consumed_tasks = 0; n_consumed_tasks < p.pool_size; n_consumed_tasks += p.queue_size) {

            if(rep >= p.n_warmup)
                timer.start("Kernel-CPU");
            host_insert_tasks(h_task_queues, h_data_queues, h_task_pool, h_data_pool, &n_written_tasks, p.queue_size,
                n_consumed_tasks, frame_size);
            if(rep >= p.n_warmup)
                timer.stop("Kernel-CPU");

            if(rep >= p.n_warmup)
                timer.start("Copy To Device");
            clStatus = clEnqueueWriteBuffer(
                ocl.clCommandQueue, d_task_queues, CL_TRUE, 0, p.queue_size * sizeof(task_t), h_task_queues, 0, NULL, 
                NULL);
            clStatus = clEnqueueWriteBuffer(
                ocl.clCommandQueue, d_data_queues, CL_TRUE, 0, p.queue_size * frame_size * sizeof(int), h_data_queues, 
                0, NULL, NULL);
            clStatus = clEnqueueWriteBuffer(
                ocl.clCommandQueue, d_histo_queues, CL_TRUE, 0, p.queue_size * p.n_bins * sizeof(int), h_histo_queues, 
                0, NULL, NULL);
            clStatus = clEnqueueWriteBuffer(
                ocl.clCommandQueue, d_consumed, CL_TRUE, 0, sizeof(int), h_consumed, 0, NULL, NULL);
            CL_ERR();
            if(rep >= p.n_warmup)
                timer.stop("Copy To Device");

            if(rep >= p.n_warmup)
                timer.start("Kernel-FPGA");
            // Setting kernel arguments
            clSetKernelArg(ocl.clKernel, 0, sizeof(task_t *), &d_task_queues);
            clSetKernelArg(ocl.clKernel, 1, sizeof(int *), &d_data_queues);
            clSetKernelArg(ocl.clKernel, 2, sizeof(int *), &d_histo_queues);
            clSetKernelArg(ocl.clKernel, 3, sizeof(int), &n_consumed_tasks);
            clSetKernelArg(ocl.clKernel, 4, sizeof(int), &p.queue_size);
            clSetKernelArg(ocl.clKernel, 5, sizeof(int), &frame_size);
            clSetKernelArg(ocl.clKernel, 6, sizeof(int), &p.n_bins);
            
			// Kernel launch
            assert(ls[0] <= max_wi && 
                "The work-group size is greater than the maximum work-group size that can be used to execute this kernel");
            clStatus     = clEnqueueTask(ocl.clCommandQueue, ocl.clKernel, 0, NULL, NULL);
            CL_ERR();
            clFinish(ocl.clCommandQueue);
            if(rep >= p.n_warmup)
                timer.stop("Kernel-FPGA");

            if(rep >= p.n_warmup)
                timer.start("Copy Back and Merge");
            clStatus = clEnqueueReadBuffer(
                ocl.clCommandQueue, d_histo_queues, CL_TRUE, 0, p.queue_size * p.n_bins * sizeof(int),
                &h_histo[n_consumed_tasks * p.n_bins], 0, NULL, NULL);
            CL_ERR();
            if(rep >= p.n_warmup)
                timer.stop("Copy Back and Merge");
        }
    }
    timer.print("Copy To Device", p.n_reps);
    timer.print("Kernel-CPU", p.n_reps);
	timer.print("Kernel-FPGA", p.n_reps);
    timer.print("Copy Back and Merge", p.n_reps);

    // Verify answer
    verify(h_histo, h_data_pool, p.pool_size, frame_size, p.n_bins);

    // Free memory
    timer.start("Deallocation");
    clStatus = clReleaseMemObject(d_task_queues);
    clStatus = clReleaseMemObject(d_data_queues);
    clStatus = clReleaseMemObject(d_histo_queues);
    clStatus = clReleaseMemObject(d_consumed);
    CL_ERR();
    free(h_consumed);
    _aligned_free(h_task_queues);
    _aligned_free(h_data_queues);
    _aligned_free(h_histo_queues);
    _aligned_free(h_task_pool);
    _aligned_free(h_data_pool);
    _aligned_free(h_histo);
    ocl.release();
    timer.stop("Deallocation");
    timer.print("Deallocation", 1);

    printf("\nTest Passed\n");
    return 0;
}

