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

// support array for sky back ground
static uint8_t sky[SCREEN_HEIGHT][SCREEN_WIDTH][3] = {0};

// support array to store final render
static uint8_t fimg[SCREEN_HEIGHT][SCREEN_WIDTH][3] = {0};

// virtual space to render surface mapping parameters
static double p2ip = 0.0060;
static double d2s = 2, dinc = 0.5, dbase;
static int dlim = 1600;
//static double hvspan[SCREEN_WIDTH], vvspan[SCREEN_HEIGHT][SCREEN_WIDTH];  discarded due to trinomentry optimization
// angles not needed anymore. sin & cos values of angles enough

// optimization variables
static double dvar_co[SCREEN_HEIGHT][SCREEN_WIDTH][3];
static double dinc_co[SCREEN_HEIGHT][SCREEN_WIDTH][3];
static double *dvar_ptr_x=NULL, *dinc_ptr_x=NULL;
static double *dvar_ptr_y=NULL, *dinc_ptr_y=NULL;
static double *dvar_ptr_z=NULL, *dinc_ptr_z=NULL;
static uint8_t *fimg_ptr = NULL;
//static double phi=0, theta=0;  discarded due to trignometry optimization
static double cos_orih, sin_orih, cos_oriv, sin_oriv;
static double cos_theta, sin_theta, cos_phi, sin_phi, op_buf;
static double sin_hvspan[SCREEN_WIDTH], sin_vvspan[SCREEN_HEIGHT][SCREEN_WIDTH];
static double cos_hvspan[SCREEN_WIDTH], cos_vvspan[SCREEN_HEIGHT][SCREEN_WIDTH];


void rasterize(){
    memcpy(fimg, sky, sizeof(sky));
    int i, j, k;
    sin_oriv = sin(oriv);
    cos_oriv = cos(oriv);
    sin_orih = sin(orih);
    cos_orih = cos(orih);
    for(j=0;j<SCREEN_WIDTH;j++){
        // theta = orih+hvspan[j];
        cos_theta = (cos_orih*cos_hvspan[j])-(sin_orih*sin_hvspan[j]);
        sin_theta = (cos_orih*sin_hvspan[j])+(sin_orih*cos_hvspan[j]);
        for(i=0;i<SCREEN_HEIGHT;i++){
            // phi = oriv+vvspan[i][j];
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
    unsigned short xt, yt;
    dvar_ptr_x = &dvar_co[0][0][0];
    dinc_ptr_x = &dinc_co[0][0][0];
    fimg_ptr = &fimg[0][0][0];
    for(i=0;i<PIXEL_COUNT;i++){
        dvar_ptr_y = dvar_ptr_x+1;
        dinc_ptr_y = dinc_ptr_x+1;
        dvar_ptr_z = dvar_ptr_x+2;
        dinc_ptr_z = dinc_ptr_x+2;
        k=0;
        while(k<dlim){
            xt = (unsigned short)(*dvar_ptr_x)%MAP_WIDTH;
            yt = (unsigned short)(*dvar_ptr_y)%MAP_HEIGHT;
            if(himgarr[yt][xt]>=*dvar_ptr_z){
                memcpy(fimg_ptr, cimgarr[yt][xt], 3);
                break;
            }
            else{
                *dvar_ptr_x += *dinc_ptr_x;                       // x
                *dvar_ptr_y += *dinc_ptr_y;                       // y
                *dvar_ptr_z += *dinc_ptr_z;                       // z
                k++;
            }
        }
        dvar_ptr_x += 3;
        dinc_ptr_x += 3;
        fimg_ptr += 3;
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

    // initialize sky array
    for(i=0;i<SCREEN_HEIGHT;i++){
        for(j=0;j<SCREEN_WIDTH;j++){
            sky[i][j][0] = 135;
            sky[i][j][1] = 206;
            sky[i][j][2] = 235;
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
