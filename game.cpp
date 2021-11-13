#include <SDL2/SDL.h>
#include <chrono>
#include <math.h>
#include <iostream>
#include "maps/map25.h"


// preset parameters
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define PIXEL_COUNT 921600

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
static double dvar_x, dvar_y;
static double dinc_x, dinc_y;
static double dvar_z[SCREEN_HEIGHT], dinc_z[SCREEN_HEIGHT];
static uint8_t sky[3] = {135, 206, 235};
static double cos_orih, sin_orih, tan_oriv;
static double cos_theta, sin_theta, tan_phi;
static double sin_hvspan[SCREEN_WIDTH], cos_hvspan[SCREEN_WIDTH];
static double tan_vvspan[SCREEN_WIDTH][SCREEN_HEIGHT];


void rasterize(){
    int i, j, k;
    // height begin and end parameters
    int hb, he, t;
    unsigned short xt, yt;
    uint8_t *cptr;
    double *hptr;
    tan_oriv = tan(oriv);
    sin_orih = sin(orih);
    cos_orih = cos(orih);
    for(j=0;j<SCREEN_WIDTH;j++){
        cos_theta = (cos_orih*cos_hvspan[j])-(sin_orih*sin_hvspan[j]);
        sin_theta = (cos_orih*sin_hvspan[j])+(sin_orih*cos_hvspan[j]);
        
        dvar_x = x + dbase*cos_theta;                  // x
        dinc_x = dinc*cos_theta;                       // x
        
        dvar_y = y + dbase*sin_theta;                  // y
        dinc_y = dinc*sin_theta;                       // y
        for(i=0;i<SCREEN_HEIGHT;i++){
            tan_phi = (tan_oriv+tan_vvspan[j][i])/(1-tan_oriv*tan_vvspan[j][i]);
            
            dvar_z[i] = z + dbase*tan_phi;                 // z
            dinc_z[i] = dinc*tan_phi;                      // z
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
            while(i>=hb && (dvar_z[i]+k*dinc_z[i])<=(*hptr)){
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
    
    return true;
}

void my_clean(){
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
        rasterize();
        x += 1;
    }
    auto t2 = high_resolution_clock::now();

    /* Getting number of milliseconds as a double. */
    duration<double, std::milli> du1 = (t2 - t1)/50;
    std::cout << du1.count() << "ms\n";

    my_clean();
    return 0;
}
