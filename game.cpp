#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <chrono>
#include <math.h>
#include <iostream>
#include "maps/map25.h"


// preset parameters
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define PIXEL_COUNT 921600
#define NUMBER_OF_THREADS 4
#define PIXEL_COUNT_PER_THREAD 230400
//PIXEL_COUNT/NUMBER_OF_THREADS

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
static double dvar_co[SCREEN_HEIGHT][SCREEN_WIDTH][3];
static double dinc_co[SCREEN_HEIGHT][SCREEN_WIDTH][3];
static double cos_orih, sin_orih, cos_oriv, sin_oriv;
static double cos_theta, sin_theta, cos_phi, sin_phi, op_buf;
static double sin_hvspan[SCREEN_WIDTH], sin_vvspan[SCREEN_HEIGHT][SCREEN_WIDTH];
static double cos_hvspan[SCREEN_WIDTH], cos_vvspan[SCREEN_HEIGHT][SCREEN_WIDTH];

// threading parameters
typedef struct{
    int sh;  // starting height
    int eh;  // ending height
    bool updated=false;
    bool active=true;
} ThreadData;
ThreadData threads_data[NUMBER_OF_THREADS];
SDL_Thread *threads[NUMBER_OF_THREADS];


void optimizer_compute(){
    int i, j;
    sin_oriv = sin(oriv);
    cos_oriv = cos(oriv);
    sin_orih = sin(orih);
    cos_orih = cos(orih);
    for(j=0;j<SCREEN_WIDTH;j++){
        cos_theta = (cos_orih*cos_hvspan[j])-(sin_orih*sin_hvspan[j]);
        sin_theta = (cos_orih*sin_hvspan[j])+(sin_orih*cos_hvspan[j]);
        for(i=0;i<SCREEN_HEIGHT;i++){
            cos_phi = (cos_oriv*cos_vvspan[i][j])-(sin_oriv*sin_vvspan[i][j]);
            sin_phi = (sin_oriv*cos_vvspan[i][j])+(cos_oriv*sin_vvspan[i][j]);
            op_buf = cos_theta*cos_phi;
            dvar_co[i][j][0] = x + dbase*op_buf;                  // x
            dinc_co[i][j][0] = dinc*op_buf;                       // x
            op_buf = sin_theta*cos_phi;
            dvar_co[i][j][1] = y + dbase*op_buf;                  // y
            dinc_co[i][j][1] = dinc*op_buf;                       // y
            
            dvar_co[i][j][2] = z + dbase*sin_phi;                 // z
            dinc_co[i][j][2] = dinc*sin_phi;                      // z
        }
    }
}


int rasterize_thread(void* data){
    ThreadData *tdata = (ThreadData*)data;
    int i, k;
    int xt, yt;
    uint8_t *fimg_ptr;
    double *dvar_ptr_x, *dvar_ptr_y, *dvar_ptr_z;
    double *dinc_ptr_x, *dinc_ptr_y, *dinc_ptr_z;
    while(tdata->active){
        if(tdata->updated){
            dvar_ptr_x = &dvar_co[tdata->sh][0][0];
            dinc_ptr_x = &dinc_co[tdata->sh][0][0];
            fimg_ptr = &fimg[tdata->sh][0][0];
            for(i=0;i<PIXEL_COUNT_PER_THREAD;i++){
                dvar_ptr_y = dvar_ptr_x+1;
                dinc_ptr_y = dinc_ptr_x+1;
                dvar_ptr_z = dvar_ptr_x+2;
                dinc_ptr_z = dinc_ptr_x+2;
                fimg_ptr[0] = 135;
                fimg_ptr[1] = 206;
                fimg_ptr[2] = 235;
                for(k=0;k<dlim;k++){
                    xt = ((int)(*dvar_ptr_x)%MAP_WIDTH+MAP_WIDTH)%MAP_WIDTH;
                    yt = ((int)(*dvar_ptr_y)%MAP_HEIGHT+MAP_HEIGHT)%MAP_HEIGHT;
                    if(himgarr[yt][xt]>=*dvar_ptr_z){
                        memcpy(fimg_ptr, cimgarr[yt][xt], 3);
                        break;
                    }
                    else{
                        *dvar_ptr_x += *dinc_ptr_x;                       // x
                        *dvar_ptr_y += *dinc_ptr_y;                       // y
                        *dvar_ptr_z += *dinc_ptr_z;                       // z
                    }
                }
                dvar_ptr_x += 3;
                dinc_ptr_x += 3;
                fimg_ptr += 3;
            }
            tdata->updated = false;
        }
        else{
            SDL_Delay(10);
        }
    }
    return 1;
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
    
    // threads setup
    int sh =0, dh = SCREEN_HEIGHT/NUMBER_OF_THREADS;
    for(i=0;i<NUMBER_OF_THREADS;i++){
        threads_data[i].sh = sh;
        threads_data[i].eh = sh+dh;
        threads_data[i].updated = false;
        threads_data[i].active = true;
        sh += dh;
    }
    
    for(i=0;i<NUMBER_OF_THREADS;i++){
        threads[i] = SDL_CreateThread(rasterize_thread, std::to_string(i).c_str(), &threads_data[i]);
    }
    
    return true;
}

void my_clean(){
    int j;
    for(j=0;j<NUMBER_OF_THREADS;j++){
        threads_data[j].active = false;
        SDL_WaitThread(threads[j], NULL);
    }
    
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
    int j;
    auto t1 = high_resolution_clock::now();
    for(i=0;i<50;i++){
        optimizer_compute();
        for(j=0;j<NUMBER_OF_THREADS;j++){
            threads_data[j].updated = true;
        }
        for(j=0;j<NUMBER_OF_THREADS;j++){
            while(threads_data[j].updated){
                SDL_Delay(10);
            }
        }
        gRenderSurface = SDL_CreateRGBSurfaceFrom((void*)fimg, SCREEN_WIDTH, SCREEN_HEIGHT, 24, 3*SCREEN_WIDTH, 0x000000ff, 0x0000ff00, 0x00ff0000, 0);
        SDL_BlitSurface(gRenderSurface, NULL, gScreenSurface, NULL);
        SDL_UpdateWindowSurface(gWindow);
        x += 1;
    }
    auto t2 = high_resolution_clock::now();

    /* Getting number of milliseconds as a double. */
    duration<double, std::milli> du1 = (t2 - t1)/50;
    std::cout << du1.count() << "ms\n";

    my_clean();
    return 0;
}
