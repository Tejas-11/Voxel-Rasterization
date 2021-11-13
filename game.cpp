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
static double dvar_z[NUMBER_OF_THREADS][SCREEN_HEIGHT], dinc_z[NUMBER_OF_THREADS][SCREEN_HEIGHT];
static uint8_t sky[3] = {135, 206, 235};
static double sin_hvspan[SCREEN_WIDTH], cos_hvspan[SCREEN_WIDTH];
static double tan_vvspan[SCREEN_WIDTH][SCREEN_HEIGHT];

// threading parameters
typedef struct{
    int id;  // thread id
    int sw;  // starting width
    int ew;  // ending width
    bool updated=false;
    bool active=true;
} ThreadData;
ThreadData threads_data[NUMBER_OF_THREADS];
SDL_Thread *threads[NUMBER_OF_THREADS];


int thread_rasterize(void* data){
    ThreadData *tdata = (ThreadData*)data;
    int i, j, k;
    // height begin and end parameters
    int hb, he, t;
    unsigned short xt, yt;
    uint8_t *cptr;
    double *hptr;
    double dvar_x, dvar_y;
    double dinc_x, dinc_y;
    double cos_theta, sin_theta, tan_phi;
    double cos_orih, sin_orih, tan_oriv;
    double *dzvar, *dzinc;
    dzvar = dvar_z[tdata->id];
    dzinc = dinc_z[tdata->id];
    int sw, ew;
    sw = tdata->sw;
    ew = tdata->ew;
    while(tdata->active){
        if(tdata->updated){
            tan_oriv = tan(oriv);
            sin_orih = sin(orih);
            cos_orih = cos(orih);
            for(j=sw;j<ew;j++){
                cos_theta = (cos_orih*cos_hvspan[j])-(sin_orih*sin_hvspan[j]);
                sin_theta = (cos_orih*sin_hvspan[j])+(sin_orih*cos_hvspan[j]);
                
                dvar_x = x + dbase*cos_theta;                  // x
                dinc_x = dinc*cos_theta;                       // x
                
                dvar_y = y + dbase*sin_theta;                  // y
                dinc_y = dinc*sin_theta;                       // y
                for(i=0;i<SCREEN_HEIGHT;i++){
                    tan_phi = (tan_oriv+tan_vvspan[j][i])/(1-tan_oriv*tan_vvspan[j][i]);
                    
                    dzvar[i] = z + dbase*tan_phi;                 // z
                    dzinc[i] = dinc*tan_phi;                      // z
                }
                k=0;
                hb = 0;
                he = SCREEN_HEIGHT-1;
                i = he;
                while(k<dlim){
                    xt = (unsigned short)(dvar_x)%MAP_WIDTH;
                    yt = (unsigned short)(dvar_y)%MAP_HEIGHT;
                    t = yt*MAP_WIDTH + xt;
                    cptr = &cimgarr[0][0][0] + t*3;
                    hptr = &himgarr[0][0] + t;
                    while(i>=hb && (dzvar[i]+k*dzinc[i])<=(*hptr)){
                        memcpy(fimg[i][j], cptr, 3);
                        i--;
                    }
                    if(i==-1){
                        break;
                    }
                    k++;
                    dvar_x += dinc_x;
                    dvar_y += dinc_y;
                }
                if(k==dlim){
                    for(;i>=hb;i--){
                        memcpy(fimg[i][j], sky, 3);
                    }
                }
            }
            tdata->updated = false;
        }
        else{
            SDL_Delay(0);
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
    double hvspan;
    for(j=0;j<=twl;j++){
        t1 = wl-j;
        
        hvspan = atan((t1*p2ip)/d2s);
        
        sin_hvspan[j] = sin(hvspan);
        sin_hvspan[SCREEN_WIDTH-j-1] = -sin_hvspan[j];
        
        cos_hvspan[j] = cos(hvspan);
        cos_hvspan[SCREEN_WIDTH-j-1] = cos_hvspan[j];
        for(i=0;i<=thl;i++){
            t2 = hl-i;
            
            tan_vvspan[j][i] = (t2*p2ip)/sqrt(pow(t1*p2ip, 2)+pow(d2s, 2));
            tan_vvspan[SCREEN_WIDTH-j-1][i] = tan_vvspan[j][i];
            tan_vvspan[j][SCREEN_HEIGHT-i-1] = -tan_vvspan[j][i];
            tan_vvspan[SCREEN_WIDTH-j-1][SCREEN_HEIGHT-i-1] = -tan_vvspan[j][i];
        }
    }
    
    // compute dbase
    dbase = sqrt(pow(p2ip*wl, 2) + pow(d2s, 2));
    
    // threads setup
    int sw =0, dw = SCREEN_WIDTH/NUMBER_OF_THREADS, mw = SCREEN_WIDTH%NUMBER_OF_THREADS;
    for(i=0;i<NUMBER_OF_THREADS;i++){
        threads_data[i].updated = false;
        threads_data[i].active = true;
        threads_data[i].sw = sw;
        threads_data[i].id = i;
        if(i<mw){
            sw += dw+1;
            threads_data[i].ew = sw;
        }
        else{
            sw += dw;
            threads_data[i].ew = sw;
        }
    }
    
    for(i=0;i<NUMBER_OF_THREADS;i++){
        threads[i] = SDL_CreateThread(thread_rasterize, std::to_string(i).c_str(), &threads_data[i]);
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
        for(j=0;j<NUMBER_OF_THREADS;j++){
            threads_data[j].updated = true;
        }
        for(j=0;j<NUMBER_OF_THREADS;j++){
            while(threads_data[j].updated){
                SDL_Delay(0);
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
