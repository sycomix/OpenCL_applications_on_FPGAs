#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <string>
#include "kmeans.h"
#include "timer.h"

#include <windows.h>

#define AOCL_ALIGNMENT 64
#include <malloc.h>

#ifndef FLT_MAX
#define FLT_MAX 3.40282347e+38
#endif
#ifdef RD_WG_SIZE_0_0
        #define BLOCK_SIZE RD_WG_SIZE_0_0
#elif defined(RD_WG_SIZE_0)
        #define BLOCK_SIZE RD_WG_SIZE_0
#elif defined(RD_WG_SIZE)
        #define BLOCK_SIZE RD_WG_SIZE
#else
        #define BLOCK_SIZE 256
#endif
#ifdef RD_WG_SIZE_1_0
     #define BLOCK_SIZE2 RD_WG_SIZE_1_0
#elif defined(RD_WG_SIZE_1)
     #define BLOCK_SIZE2 RD_WG_SIZE_1
#elif defined(RD_WG_SIZE)
     #define BLOCK_SIZE2 RD_WG_SIZE
#else
     #define BLOCK_SIZE2 256
#endif

#include <CL/cl.h>
#include <fstream>
#include <iostream>

#include "CL/opencl.h"
#include "AOCLUtils/aocl_utils.h"

using namespace aocl_utils;

// Allocation error checking
#define ERR_1(v1)                                                                                                      \
    if(v1 == NULL) {                                                                                                   \
        fprintf(stderr, "Allocation error at %s, %d\n", __FILE__, __LINE__);                                           \
        exit(-1);                                                                                                      \
    }
#define ERR_2(v1,v2) ERR_1(v1) ERR_1(v2)
#define ERR_3(v1,v2,v3) ERR_2(v1,v2) ERR_1(v3)
#define ERR_4(v1,v2,v3,v4) ERR_3(v1,v2,v3) ERR_1(v4)
#define ERR_5(v1,v2,v3,v4,v5) ERR_4(v1,v2,v3,v4) ERR_1(v5)
#define ERR_6(v1,v2,v3,v4,v5,v6) ERR_5(v1,v2,v3,v4,v5) ERR_1(v6)
#define GET_ERR_MACRO(_1,_2,_3,_4,_5,_6,NAME,...) NAME
#define ALLOC_ERR(...) GET_ERR_MACRO(__VA_ARGS__,ERR_6,ERR_5,ERR_4,ERR_3,ERR_2,ERR_1)(__VA_ARGS__)

#define CL_ERR()                                                                                                       \
    if(clStatus != CL_SUCCESS) {                                                                                       \
        fprintf(stderr, "OpenCL error: %d\n at %s, %d\n", clStatus, __FILE__, __LINE__);                               \
        exit(-1);                                                                                                      \
    }



// local variables
static cl_context	    clContext;
static cl_command_queue clCommandQueue_in;
static cl_command_queue clCommandQueue_0;
static cl_command_queue clCommandQueue_1;

static cl_command_queue clCommandQueue_2;
static cl_command_queue clCommandQueue_3;

static cl_command_queue clCommandQueue_4;
static cl_command_queue clCommandQueue_5;
static cl_command_queue clCommandQueue_6;
static cl_command_queue clCommandQueue_7;

static cl_program       clProgram;
static cl_kernel        clKernel_in;
static cl_kernel        clKernel_0;
static cl_kernel        clKernel_1;

static cl_kernel        clKernel_2;
static cl_kernel        clKernel_3;

static cl_kernel        clKernel_4;
static cl_kernel        clKernel_5;
static cl_kernel        clKernel_6;
static cl_kernel        clKernel_7;

static cl_device_id		clDeviceID;

Timer timer;

static int shutdown()
{
	// release resources
	if( clCommandQueue_in ) clReleaseCommandQueue( clCommandQueue_in );
	if( clCommandQueue_0 ) clReleaseCommandQueue( clCommandQueue_0 );
	if( clCommandQueue_1 ) clReleaseCommandQueue( clCommandQueue_1 );
	
	if( clCommandQueue_2 ) clReleaseCommandQueue( clCommandQueue_2 );
	if( clCommandQueue_3 ) clReleaseCommandQueue( clCommandQueue_3 );
	
	if( clCommandQueue_4 ) clReleaseCommandQueue( clCommandQueue_4 );
	if( clCommandQueue_5 ) clReleaseCommandQueue( clCommandQueue_5 );
	if( clCommandQueue_6 ) clReleaseCommandQueue( clCommandQueue_6 );
	if( clCommandQueue_7 ) clReleaseCommandQueue( clCommandQueue_7 );
	
	if( clContext ) clReleaseContext( clContext );
	// reset all variables
	clCommandQueue_in = 0;
	clCommandQueue_0 = 0;
	clCommandQueue_1 = 0;
	
	clCommandQueue_2 = 0;
	clCommandQueue_3 = 0;
	
	clCommandQueue_4 = 0;
	clCommandQueue_5 = 0;
	clCommandQueue_6 = 0;
	clCommandQueue_7 = 0;
	
	clContext = 0;
	return 0;
}


cl_mem d_feature;
cl_mem d_cluster;
cl_mem d_membership;

float *feature_swap;
int   *membership_OCL;
int   *membership_d;
float *feature_d;
float *clusters_d;
float *center_d;


int allocate(int n_points, int n_features, int n_clusters, float **feature)
{
	cl_int  clStatus;


	// Get the OpenCL platform.
	cl_platform_id clPlatform = NULL;
	clPlatform = findPlatform("Intel(R) FPGA");
	if(clPlatform == NULL) {
		printf("ERROR: Unable to find Intel(R) FPGA OpenCL platform.\n");
	}

	// Query the available OpenCL device.
    cl_uint clNumDevices;
    clStatus = clGetDeviceIDs(clPlatform, CL_DEVICE_TYPE_ALL, 0, NULL, &clNumDevices);
    CL_ERR();
    cl_device_id *clDevices = new cl_device_id[clNumDevices];
    clStatus                = clGetDeviceIDs(clPlatform, CL_DEVICE_TYPE_ALL, clNumDevices, clDevices, NULL);
    CL_ERR();

	printf("Platform: %s\n",getPlatformName(clPlatform).c_str());
	printf("Using %d device(s)\n",clNumDevices);
	for(unsigned i = 0; i < clNumDevices; ++i) {
		printf("  %s\n", getDeviceName(clDevices[i]).c_str());
	}


	// Create the context.
	clContext = clCreateContext(NULL, clNumDevices, clDevices, &oclContextCallback, NULL, &clStatus);
    CL_ERR();
    char device_name_[100];
    clGetDeviceInfo(clDevices[0], CL_DEVICE_NAME, 100, &device_name_, NULL);
    clDeviceID = clDevices[0];
    fprintf(stderr, "%s\t", device_name_);


	
	// create command queue for the first device
	clCommandQueue_in = clCreateCommandQueue( clContext, clDevices[0], 0, &clStatus );
	clCommandQueue_0 = clCreateCommandQueue( clContext, clDevices[0], 0, &clStatus );
	clCommandQueue_1 = clCreateCommandQueue( clContext, clDevices[0], 0, &clStatus );
	
	clCommandQueue_2 = clCreateCommandQueue( clContext, clDevices[0], 0, &clStatus );
	clCommandQueue_3 = clCreateCommandQueue( clContext, clDevices[0], 0, &clStatus );
	
	clCommandQueue_4 = clCreateCommandQueue( clContext, clDevices[0], 0, &clStatus );
	clCommandQueue_5 = clCreateCommandQueue( clContext, clDevices[0], 0, &clStatus );
	clCommandQueue_6 = clCreateCommandQueue( clContext, clDevices[0], 0, &clStatus );
	clCommandQueue_7 = clCreateCommandQueue( clContext, clDevices[0], 0, &clStatus );
	
	CL_ERR();



	// Create the program.
	std::string binary_file = getBoardBinaryFile("cluster_1-8", clDeviceID);
	printf("\nUsing AOCX:%s\n",binary_file.c_str());
	clProgram = createProgramFromBinary(clContext, binary_file.c_str(), &clDeviceID, 1);	
	CL_ERR();


    char clOptions[50];
    sprintf(clOptions, "-I.");

	// Build the program that was just created.
    clStatus = clBuildProgram(clProgram, 0, NULL, "", NULL, NULL);  
    if(clStatus == CL_BUILD_PROGRAM_FAILURE) {
        // Determine the size of the log
        size_t log_size;
        clGetProgramBuildInfo(clProgram, clDevices[0], CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        // Allocate memory for the log
        char *log = (char *)malloc(log_size);
        // Get the log
        clGetProgramBuildInfo(clProgram, clDevices[0], CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        // Print the log
        fprintf(stderr, "%s\t", log);
    }
    CL_ERR();



	// Kernel.
	clKernel_in  = clCreateKernel(clProgram, "kmeans_in", &clStatus);
    clKernel_0  = clCreateKernel(clProgram, "kmeans_kernel_0", &clStatus);
	clKernel_1  = clCreateKernel(clProgram, "kmeans_kernel_1", &clStatus);
    
	clKernel_2  = clCreateKernel(clProgram, "kmeans_kernel_2", &clStatus);
	clKernel_3  = clCreateKernel(clProgram, "kmeans_kernel_3", &clStatus);
    
	clKernel_4  = clCreateKernel(clProgram, "kmeans_kernel_4", &clStatus);
	clKernel_5  = clCreateKernel(clProgram, "kmeans_kernel_5", &clStatus);
    clKernel_6  = clCreateKernel(clProgram, "kmeans_kernel_6", &clStatus);
	clKernel_7  = clCreateKernel(clProgram, "kmeans_kernel_7", &clStatus);
	
    CL_ERR();


	d_feature = clCreateBuffer(clContext, CL_MEM_READ_WRITE, n_points * n_features * sizeof(float), NULL, 0 );
	d_cluster = clCreateBuffer(clContext, CL_MEM_READ_WRITE, n_clusters * n_features  * sizeof(float), NULL, 0 );
	d_membership = clCreateBuffer(clContext, CL_MEM_READ_WRITE, n_points * sizeof(int), NULL, 0 );
	

	feature_swap = (float*) _aligned_malloc (n_points * n_features * sizeof(float), AOCL_ALIGNMENT);
	for(int j = 0; j < n_points; j ++){
		for(int i = 0; i < n_features; i ++){
			feature_swap[i * n_points + j] = feature[j][i];
		}
	}


	//write buffers
	clStatus = clEnqueueWriteBuffer(clCommandQueue_0, d_feature, 1, 0, n_points * n_features * sizeof(float), feature_swap, 0, 0, 0);
	//clStatus = clEnqueueWriteBuffer(clCommandQueue_0, d_feature, 1, 0, n_points * n_features * sizeof(float), feature[0], 0, 0, 0);
	CL_ERR();

	membership_OCL = (int*) _aligned_malloc(n_points * sizeof(int), AOCL_ALIGNMENT);
}


void deallocateMemory()
{
	clReleaseMemObject(d_feature);
	clReleaseMemObject(d_cluster);
	clReleaseMemObject(d_membership);
	_aligned_free(feature_swap);
	_aligned_free(membership_OCL);
}


int main( int argc, char** argv) 
{
	setup(argc, argv);
	shutdown();
}


int	kmeansOCL(float **feature,    /* in: [npoints][nfeatures] */
           int     n_features,
           int     n_points,
           int     n_clusters,
           int    *membership,
		   float **clusters,
		   int     *new_centers_len,
           float  **new_centers,
		   float   *time)	
{ 
	int delta = 0;
	int i, j;
	cl_int  clStatus;

	clStatus = clEnqueueWriteBuffer(clCommandQueue_0, d_cluster, 1, 0, n_clusters * n_features * sizeof(float), clusters[0], 0, 0, 0);
	clFinish(clCommandQueue_0);
	CL_ERR();
	
	
	timer.start("Kernel");
		
	
	//clSetKernelArg(clKernel_in, 0, sizeof(void *), (void*) &d_feature);
	clSetKernelArg(clKernel_in, 0, sizeof(void *), (void*) &d_cluster);
	//clSetKernelArg(clKernel_in, 1, sizeof(void *), (void*) &d_membership);
	//clSetKernelArg(clKernel_in, 1, sizeof(cl_int), (void*) &n_points);
	clSetKernelArg(clKernel_in, 1, sizeof(cl_int), (void*) &n_clusters);
	clSetKernelArg(clKernel_in, 2, sizeof(cl_int), (void*) &n_features);

	clStatus = clEnqueueTask(clCommandQueue_in, clKernel_in, 0, 0, 0);
	CL_ERR();
	



	clSetKernelArg(clKernel_0, 0, sizeof(void *), (void*) &d_feature);
	//clSetKernelArg(clKernel_0, 0, sizeof(void *), (void*) &d_cluster);
	clSetKernelArg(clKernel_0, 1, sizeof(void *), (void*) &d_membership);
	clSetKernelArg(clKernel_0, 2, sizeof(cl_int), (void*) &n_points);
	clSetKernelArg(clKernel_0, 3, sizeof(cl_int), (void*) &n_clusters);
	clSetKernelArg(clKernel_0, 4, sizeof(cl_int), (void*) &n_features);

	clStatus = clEnqueueTask(clCommandQueue_0, clKernel_0, 0, 0, 0);
	CL_ERR();


	clSetKernelArg(clKernel_1, 0, sizeof(void *), (void*) &d_feature);
	//clSetKernelArg(clKernel_1, 0, sizeof(void *), (void*) &d_cluster);
	clSetKernelArg(clKernel_1, 1, sizeof(void *), (void*) &d_membership);
	clSetKernelArg(clKernel_1, 2, sizeof(cl_int), (void*) &n_points);
	clSetKernelArg(clKernel_1, 3, sizeof(cl_int), (void*) &n_clusters);
	clSetKernelArg(clKernel_1, 4, sizeof(cl_int), (void*) &n_features);

	clStatus = clEnqueueTask(clCommandQueue_1, clKernel_1, 0, 0, 0);
	CL_ERR();

	
	clSetKernelArg(clKernel_2, 0, sizeof(void *), (void*) &d_feature);
	//clSetKernelArg(clKernel_2, 0, sizeof(void *), (void*) &d_cluster);
	clSetKernelArg(clKernel_2, 1, sizeof(void *), (void*) &d_membership);
	clSetKernelArg(clKernel_2, 2, sizeof(cl_int), (void*) &n_points);
	clSetKernelArg(clKernel_2, 3, sizeof(cl_int), (void*) &n_clusters);
	clSetKernelArg(clKernel_2, 4, sizeof(cl_int), (void*) &n_features);

	clStatus = clEnqueueTask(clCommandQueue_2, clKernel_2, 0, 0, 0);
	CL_ERR();


	clSetKernelArg(clKernel_3, 0, sizeof(void *), (void*) &d_feature);
	//clSetKernelArg(clKernel_3, 0, sizeof(void *), (void*) &d_cluster);
	clSetKernelArg(clKernel_3, 1, sizeof(void *), (void*) &d_membership);
	clSetKernelArg(clKernel_3, 2, sizeof(cl_int), (void*) &n_points);
	clSetKernelArg(clKernel_3, 3, sizeof(cl_int), (void*) &n_clusters);
	clSetKernelArg(clKernel_3, 4, sizeof(cl_int), (void*) &n_features);

	clStatus = clEnqueueTask(clCommandQueue_3, clKernel_3, 0, 0, 0);
	CL_ERR();

	
	clSetKernelArg(clKernel_4, 0, sizeof(void *), (void*) &d_feature);
	//clSetKernelArg(clKernel_4, 0, sizeof(void *), (void*) &d_cluster);
	clSetKernelArg(clKernel_4, 1, sizeof(void *), (void*) &d_membership);
	clSetKernelArg(clKernel_4, 2, sizeof(cl_int), (void*) &n_points);
	clSetKernelArg(clKernel_4, 3, sizeof(cl_int), (void*) &n_clusters);
	clSetKernelArg(clKernel_4, 4, sizeof(cl_int), (void*) &n_features);

	clStatus = clEnqueueTask(clCommandQueue_4, clKernel_4, 0, 0, 0);
	CL_ERR();


	clSetKernelArg(clKernel_5, 0, sizeof(void *), (void*) &d_feature);
	//clSetKernelArg(clKernel_5, 0, sizeof(void *), (void*) &d_cluster);
	clSetKernelArg(clKernel_5, 1, sizeof(void *), (void*) &d_membership);
	clSetKernelArg(clKernel_5, 2, sizeof(cl_int), (void*) &n_points);
	clSetKernelArg(clKernel_5, 3, sizeof(cl_int), (void*) &n_clusters);
	clSetKernelArg(clKernel_5, 4, sizeof(cl_int), (void*) &n_features);

	clStatus = clEnqueueTask(clCommandQueue_5, clKernel_5, 0, 0, 0);
	CL_ERR();

	clSetKernelArg(clKernel_6, 0, sizeof(void *), (void*) &d_feature);
	//clSetKernelArg(clKernel_6, 0, sizeof(void *), (void*) &d_cluster);
	clSetKernelArg(clKernel_6, 1, sizeof(void *), (void*) &d_membership);
	clSetKernelArg(clKernel_6, 2, sizeof(cl_int), (void*) &n_points);
	clSetKernelArg(clKernel_6, 3, sizeof(cl_int), (void*) &n_clusters);
	clSetKernelArg(clKernel_6, 4, sizeof(cl_int), (void*) &n_features);

	clStatus = clEnqueueTask(clCommandQueue_6, clKernel_6, 0, 0, 0);
	CL_ERR();


	clSetKernelArg(clKernel_7, 0, sizeof(void *), (void*) &d_feature);
	//clSetKernelArg(clKernel_7, 0, sizeof(void *), (void*) &d_cluster);
	clSetKernelArg(clKernel_7, 1, sizeof(void *), (void*) &d_membership);
	clSetKernelArg(clKernel_7, 2, sizeof(cl_int), (void*) &n_points);
	clSetKernelArg(clKernel_7, 3, sizeof(cl_int), (void*) &n_clusters);
	clSetKernelArg(clKernel_7, 4, sizeof(cl_int), (void*) &n_features);

	clStatus = clEnqueueTask(clCommandQueue_7, clKernel_7, 0, 0, 0);
	CL_ERR();

	

	clFinish(clCommandQueue_in);
	clFinish(clCommandQueue_0);
	clFinish(clCommandQueue_1);
	
	clFinish(clCommandQueue_2);
	clFinish(clCommandQueue_3);
	
	clFinish(clCommandQueue_4);
	clFinish(clCommandQueue_5);
	clFinish(clCommandQueue_6);
	clFinish(clCommandQueue_7);
	
	timer.stop("Kernel");

	time[0] = timer.getTime("Kernel");
	//timer.print("Kernel", 1);


	clStatus = clEnqueueReadBuffer(clCommandQueue_0, d_membership, 1, 0, n_points * sizeof(int), membership_OCL, 0, 0, 0);
	CL_ERR();


	delta = 0;
	for (i = 0; i < n_points; i++)
	{
		int cluster_id = membership_OCL[i];
		new_centers_len[cluster_id]++;
		if (membership_OCL[i] != membership[i])
		{
			delta++;
			membership[i] = membership_OCL[i];
		}
		for (j = 0; j < n_features; j++)
		{
			new_centers[cluster_id][j] += feature[i][j];
		}
	}


	if(delta == 0){
		int temp;
		FILE *fp = fopen("output/output.txt", "r");
		for(int i = 0; i < n_points; i++){
			fscanf(fp, "%d", &temp);
			if(membership_OCL[i] != temp){
				printf("failed!!\n");
				break;
			}
		}
		printf("pass!!\n");
	}




	return delta;
}

