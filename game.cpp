#include <SDL2/SDL.h>
#include <chrono>
#include <math.h>
#define CL_TARGET_OPENCL_VERSION 220
#include <CL/cl.h>
#include <iostream>
#include "maps/map25.h"


// preset parameters
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define PIXEL_COUNT 921600
#define PROGRAM_PATH "./pixel_loop.cl"
#define KERNEL_FUNC "pixel_loop"
#define MAX_SOURCE_SIZE (0x100000)
// confirm overlapping parameters have same values in PROGRAM_PATH


// openCL parameters
cl_mem gx, gy, gz;
cl_mem gdbase, gdinc, gdlim;
cl_mem gcos_orih, gsin_orih, gcos_oriv, gsin_oriv;
cl_mem gcos_hvspan, gsin_hvspan, gcos_vvspan, gsin_vvspan;
cl_mem ghimgarr, gcimgarr;
cl_mem gfimg;
cl_platform_id gplatform;
cl_device_id gdevice;
cl_context gcontext;
cl_command_queue gqueue;
cl_program gprogram;
cl_kernel gkernel;
char *kernelSource;
size_t localSize, globalSize, kernelSourceSize;


// sdl parameters
SDL_Window* gWindow = NULL;
SDL_Surface* gScreenSurface = NULL;
SDL_Surface* gRenderSurface = NULL;

// viewer parameters
static double x=0, y=0, z=150, orih=0, oriv=0;

// map parameters
static double h2d=1.0;
static uint8_t cimgarr[MAP_HEIGHT][MAP_WIDTH][3];
static double himgarr[MAP_HEIGHT][MAP_WIDTH];

// support array to store final render
static uint8_t fimg[SCREEN_HEIGHT][SCREEN_WIDTH][3] = {0};

// virtual space to render surface mapping parameters
static double p2ip = 0.0060;
static double d2s = 2, dinc = 0.5, dbase;
static int dlim = 1600;

// optimization variables
static double cos_orih, sin_orih, cos_oriv, sin_oriv;
static double sin_hvspan[SCREEN_WIDTH], sin_vvspan[SCREEN_HEIGHT][SCREEN_WIDTH];
static double cos_hvspan[SCREEN_WIDTH], cos_vvspan[SCREEN_HEIGHT][SCREEN_WIDTH];


void gpu_rasterize(){
    cl_int err;
    sin_oriv = sin(oriv);
    cos_oriv = cos(oriv);
    sin_orih = sin(orih);
    cos_orih = cos(orih);
    err = clEnqueueWriteBuffer(gqueue, gx, CL_TRUE, 0, sizeof(double), &x, 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gqueue, gy, CL_TRUE, 0, sizeof(double), &y, 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gqueue, gz, CL_TRUE, 0, sizeof(double), &z, 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gqueue, gcos_orih, CL_TRUE, 0, sizeof(double), &cos_orih, 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gqueue, gsin_orih, CL_TRUE, 0, sizeof(double), &sin_orih, 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gqueue, gcos_oriv, CL_TRUE, 0, sizeof(double), &cos_oriv, 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gqueue, gsin_oriv, CL_TRUE, 0, sizeof(double), &sin_oriv, 0, NULL, NULL);
    err = clEnqueueNDRangeKernel(gqueue, gkernel, 1, NULL, &globalSize, &localSize, 0, NULL, NULL);
    //printf("%d\n", err);
    clFinish(gqueue);
    err = clEnqueueReadBuffer(gqueue, gfimg, CL_TRUE, 0, PIXEL_COUNT*3, &fimg[0][0][0], 0, NULL, NULL);
    
    gRenderSurface = SDL_CreateRGBSurfaceFrom((void*)fimg, SCREEN_WIDTH, SCREEN_HEIGHT, 24, 3*SCREEN_WIDTH, 0x000000ff, 0x0000ff00, 0x00ff0000, 0);
    SDL_BlitSurface(gRenderSurface, NULL, gScreenSurface, NULL);
    SDL_UpdateWindowSurface(gWindow);
}

bool my_setup(){
    
    // SDL setup
    if(SDL_Init(SDL_INIT_VIDEO)<0){
        printf("SDL initialization error: %s\n", SDL_GetError());
        return false;
    }
    
    // spawn SDL window
    gWindow = SDL_CreateWindow("Game", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if(gWindow==NULL){
        printf("SDL Window creation error: %s\n", SDL_GetError());
        return false;
    }
    gScreenSurface = SDL_GetWindowSurface(gWindow);
    
    // load color and height map images to memory
    int i, j, k;
    
    // move map data to stack arrays
    for(i=0;i<MAP_HEIGHT;i++){
        for(j=0;j<MAP_WIDTH;j++){
            himgarr[i][j] = h2d*himgart[i][j];
            for(k=0;k<3;k++){
                cimgarr[i][j][k] = cimgart[i][j][k];
            }
        }
    }
    
    // compute sin and cos of relavent angles
    double wl = (SCREEN_WIDTH-1)/2;
    double hl = (SCREEN_HEIGHT-1)/2;
    int twl = (int) wl;
    int thl = (int) hl;
    double t1, t2;
    double hvspan, vvspan;
    for(j=0;j<=twl;j++){
        t1 = wl-j;
        
        hvspan = atan((t1*p2ip)/d2s);
        
        sin_hvspan[j] = sin(hvspan);
        sin_hvspan[SCREEN_WIDTH-j-1] = -sin_hvspan[j];
        
        cos_hvspan[j] = cos(hvspan);
        cos_hvspan[SCREEN_WIDTH-j-1] = cos_hvspan[j];
        for(i=0;i<=thl;i++){
            t2 = hl-i;
            
            vvspan = atan((t2*p2ip)/sqrt(pow(t1*p2ip, 2)+pow(d2s, 2)));
            
            sin_vvspan[i][j] = sin(vvspan);
            sin_vvspan[i][SCREEN_WIDTH-j-1] = sin_vvspan[i][j];
            sin_vvspan[SCREEN_HEIGHT-i-1][j] = -sin_vvspan[i][j];
            sin_vvspan[SCREEN_HEIGHT-i-1][SCREEN_WIDTH-j-1] = -sin_vvspan[i][j];
            
            cos_vvspan[i][j] = cos(vvspan);
            cos_vvspan[i][SCREEN_WIDTH-j-1] = cos_vvspan[i][j];
            cos_vvspan[SCREEN_HEIGHT-i-1][j] = cos_vvspan[i][j];
            cos_vvspan[SCREEN_HEIGHT-i-1][SCREEN_WIDTH-j-1] = cos_vvspan[i][j];
        }
    }
    
    // compute dbase
    dbase = sqrt(pow(p2ip*hl, 2) + pow(p2ip*wl, 2) + pow(d2s, 2));
    
    // load accelerated code from PROGRAM_PATH
    FILE *fp;
    fp = fopen(PROGRAM_PATH, "r");
    if(!fp){
        printf("failed to load accelerated program code\n");
        return false;
    }
    kernelSource = (char*)malloc(MAX_SOURCE_SIZE);
    kernelSourceSize = fread(kernelSource, 1, MAX_SOURCE_SIZE, fp);
    fclose(fp);
    //printf("%s\n", kernelSource);

    // initailize local & global size for accelerated code
    localSize = 64;
    globalSize = ceil(PIXEL_COUNT/(float)localSize)*localSize;
    
    // openCL setup
    cl_int err;
    err = clGetPlatformIDs(1, &gplatform, NULL);
    //printf("%d\n", err);
    err = clGetDeviceIDs(gplatform, CL_DEVICE_TYPE_DEFAULT, 1, &gdevice, NULL);
    //printf("%d\n", err);
    gcontext = clCreateContext(NULL, 1, &gdevice, NULL, NULL, &err);
    //printf("%d\n", err);
    gqueue = clCreateCommandQueueWithProperties(gcontext, gdevice, 0, &err);
    //printf("%d\n", err);
    
    // build accelerated code
    gprogram = clCreateProgramWithSource(gcontext, 1, (const char **) & kernelSource, (const size_t *)&kernelSourceSize, &err);
    //printf("%d\n", err);
    err = clBuildProgram(gprogram, 1, &gdevice, NULL, NULL, NULL);
    //printf("%d\n", err);
    if(err!=CL_SUCCESS){
        char log_buffer[50000];
        size_t log_size;
        clGetProgramBuildInfo(gprogram, gdevice, CL_PROGRAM_BUILD_LOG, sizeof(log_buffer), log_buffer, &log_size);
        printf("--- BUILD ERROR LOG ---\n%s\n", log_buffer);
        return false;
    }
    gkernel = clCreateKernel(gprogram, KERNEL_FUNC, &err);
    
    // memory allocation for accelerated code
    // co-ordinate variables
    gx = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, sizeof(double), NULL, &err);
    gy = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, sizeof(double), NULL, &err);
    gz = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, sizeof(double), NULL, &err);
    // frustum variables
    gdbase = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, sizeof(double), NULL, &err);
    gdinc = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, sizeof(double), NULL, &err);
    gdlim = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, sizeof(int), NULL, &err);
    // sin cos of viwer orientation
    gcos_orih = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, sizeof(double), NULL, &err);
    gsin_orih = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, sizeof(double), NULL, &err);
    gcos_oriv = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, sizeof(double), NULL, &err);
    gsin_oriv = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, sizeof(double), NULL, &err);
    // sin cos of precomputed angles
    gcos_hvspan = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, SCREEN_WIDTH*sizeof(double), NULL, &err);
    gsin_hvspan = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, SCREEN_WIDTH*sizeof(double), NULL, &err);
    gcos_vvspan = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, PIXEL_COUNT*sizeof(double), NULL, &err);
    gsin_vvspan = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, PIXEL_COUNT*sizeof(double), NULL, &err);
    // map data
    ghimgarr = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, MAP_HEIGHT*MAP_WIDTH*sizeof(double), NULL, &err);
    gcimgarr = clCreateBuffer(gcontext, CL_MEM_READ_ONLY, MAP_HEIGHT*MAP_WIDTH*3, NULL, &err);
    // final rasterize output buffer
    gfimg = clCreateBuffer(gcontext, CL_MEM_WRITE_ONLY, PIXEL_COUNT*3, NULL, &err);
    
    // fill memory buffer for accelerated code with data
    // frustum data
    err = clEnqueueWriteBuffer(gqueue, gdbase, CL_TRUE, 0, sizeof(double), &dbase, 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gqueue, gdinc, CL_TRUE, 0, sizeof(double), &dinc, 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gqueue, gdlim, CL_TRUE, 0, sizeof(int), &dlim, 0, NULL, NULL);
    // sin cos of precomputed angles
    err = clEnqueueWriteBuffer(gqueue, gcos_hvspan, CL_TRUE, 0, SCREEN_WIDTH*sizeof(double), &cos_hvspan[0], 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gqueue, gsin_hvspan, CL_TRUE, 0, SCREEN_WIDTH*sizeof(double), &sin_hvspan[0], 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gqueue, gcos_vvspan, CL_TRUE, 0, PIXEL_COUNT*sizeof(double), &cos_vvspan[0][0], 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gqueue, gsin_vvspan, CL_TRUE, 0, PIXEL_COUNT*sizeof(double), &sin_vvspan[0][0], 0, NULL, NULL);
    // map data
    err = clEnqueueWriteBuffer(gqueue, ghimgarr, CL_TRUE, 0, MAP_HEIGHT*MAP_WIDTH*sizeof(double), &himgarr[0][0], 0, NULL, NULL);
    err = clEnqueueWriteBuffer(gqueue, gcimgarr, CL_TRUE, 0, MAP_HEIGHT*MAP_WIDTH*3, &cimgarr[0][0][0], 0, NULL, NULL);
    
    // set kernal arguments
    err = clSetKernelArg(gkernel, 0, sizeof(cl_mem), (void *)&gx);
    err = clSetKernelArg(gkernel, 1, sizeof(cl_mem), (void *)&gy);
    err = clSetKernelArg(gkernel, 2, sizeof(cl_mem), (void *)&gz);
    err = clSetKernelArg(gkernel, 3, sizeof(cl_mem), (void *)&gdbase);
    err = clSetKernelArg(gkernel, 4, sizeof(cl_mem), (void *)&gdinc);
    err = clSetKernelArg(gkernel, 5, sizeof(cl_mem), (void *)&gdlim);
    err = clSetKernelArg(gkernel, 6, sizeof(cl_mem), (void *)&gcos_orih);
    err = clSetKernelArg(gkernel, 7, sizeof(cl_mem), (void *)&gsin_orih);
    err = clSetKernelArg(gkernel, 8, sizeof(cl_mem), (void *)&gcos_oriv);
    err = clSetKernelArg(gkernel, 9, sizeof(cl_mem), (void *)&gsin_oriv);
    err = clSetKernelArg(gkernel, 10, sizeof(cl_mem), (void *)&gcos_hvspan);
    err = clSetKernelArg(gkernel, 11, sizeof(cl_mem), (void *)&gsin_hvspan);
    err = clSetKernelArg(gkernel, 12, sizeof(cl_mem), (void *)&gcos_vvspan);
    err = clSetKernelArg(gkernel, 13, sizeof(cl_mem), (void *)&gsin_vvspan);
    err = clSetKernelArg(gkernel, 14, sizeof(cl_mem), (void *)&ghimgarr);
    err = clSetKernelArg(gkernel, 15, sizeof(cl_mem), (void *)&gcimgarr);
    err = clSetKernelArg(gkernel, 16, sizeof(cl_mem), (void *)&gfimg);
    
    return true;
}

void my_clean(){
    // clean openCL
    clFlush(gqueue);
    clReleaseKernel(gkernel);
    clReleaseProgram(gprogram);
    clReleaseMemObject(gx);
    clReleaseMemObject(gy);
    clReleaseMemObject(gz);
    clReleaseMemObject(gdbase);
    clReleaseMemObject(gdinc);
    clReleaseMemObject(gdlim);
    clReleaseMemObject(gcos_orih);
    clReleaseMemObject(gsin_orih);
    clReleaseMemObject(gcos_oriv);
    clReleaseMemObject(gsin_oriv);
    clReleaseMemObject(gcos_hvspan);
    clReleaseMemObject(gsin_hvspan);
    clReleaseMemObject(gcos_vvspan);
    clReleaseMemObject(gsin_vvspan);
    clReleaseMemObject(gfimg);
    clReleaseMemObject(ghimgarr);
    clReleaseMemObject(gcimgarr);
    clReleaseCommandQueue(gqueue);
    clReleaseContext(gcontext);
    
    // clean SDL
    SDL_FreeSurface(gRenderSurface);
    gRenderSurface = NULL;
    SDL_DestroyWindow(gWindow);
    gWindow = NULL;
    SDL_Quit();
}

int main(){
    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::duration;
    using std::chrono::milliseconds;
    if(!my_setup()){
        return 0;
    }
    int i;
    auto t1 = high_resolution_clock::now();
    for(i=0;i<50;i++){
        gpu_rasterize();
        x += 1;
    }
    auto t2 = high_resolution_clock::now();

    /* Getting number of milliseconds as a double. */
    duration<double, std::milli> du1 = (t2 - t1)/50;
    std::cout << du1.count() << "ms\n";

    my_clean();
    return 0;
}
