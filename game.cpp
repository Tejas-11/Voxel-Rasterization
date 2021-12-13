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
SDL_Event e;

// viewer parameters
static double x=0, y=0, z=150, orih=0, oriv=0;

// map parameters
static double h2d=1.0;
static uint8_t cimgarr[MAP_HEIGHT][MAP_WIDTH][3];
static double himgarr[MAP_HEIGHT][MAP_WIDTH];

// support array to store final render
static uint8_t fimg[SCREEN_HEIGHT][SCREEN_WIDTH][3] = {0};

// virtual space to render surface mapping parameters
static double p2ip = 0.0040;
static double d2s = 2, dinc = 0.5, dbase;
static int dlim = 1600;

// optimization variables
static double cos_orih, sin_orih, cos_oriv, sin_oriv;
static uint8_t sky[3] = {135, 206, 235};
static double pb2 = asin(1);
static double pi = 2*pb2;
static double tpi = 2*pi;
static double wl = (SCREEN_WIDTH-1)*p2ip/2;
static double hl = (SCREEN_HEIGHT-1)*p2ip/2;

// motion parameters
static bool fwd=false, bwd=false, rht=false, lft=false;
static double spd=2.0;
static double ang_spd=0.01;
static bool quit = false;
static double vanglimu, vangliml;

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
    int xt, yt;
    uint8_t *cptr;
    double *hptr;
    double dvar_x, dvar_y;
    double dinc_x, dinc_y;
    double dvar_z, dinc_z;
    double xvb, yvb, xib, yib;
    double cos_theta, sin_theta, tan_phi;
    double di, dj, djs;
    double t1, t2, t3, sin_hv, cos_hv;
    int sw, ew;
    sw = tdata->sw;
    ew = tdata->ew;
    while(tdata->active){
        if(tdata->updated){
            sin_oriv = sin(oriv);
            cos_oriv = cos(oriv);
            sin_orih = sin(orih);
            cos_orih = cos(orih);
            dj = wl - sw*p2ip;
            for(j=sw;j<ew;j++){
                djs = dj*dj;
                
                di = -hl;
                xvb = -9999;
                yvb = -9999;
                xib = -9999;
                yib = -9999;
                for(i=SCREEN_HEIGHT-1;i>=0;i--){
                    t1 = d2s*cos_oriv - di*sin_oriv;
                    t2 = t1*t1;
                    t3 = sqrt(djs+t2);
                    sin_hv = dj/t3;
                    cos_hv = t1/t3;
                    
                    cos_theta = cos_orih*cos_hv-sin_orih*sin_hv;
                    sin_theta = cos_orih*sin_hv+sin_orih*cos_hv;
                    
                    dvar_x = x + dbase*cos_theta;                  // x
                    dinc_x = dinc*cos_theta;                       // x
                    
                    dvar_y = y + dbase*sin_theta;                  // y
                    dinc_y = dinc*sin_theta;                       // y
                
                    tan_phi = (di*cos_oriv+d2s*sin_oriv)/t3;

                    dvar_z = z + dbase*tan_phi;                 // z
                    dinc_z = dinc*tan_phi;                      // z

                    di += p2ip;
                    if((int)dvar_x!=(int)xvb || (int)dvar_y!=(int)yvb || (int)(dvar_x+k*dinc_x)!=(int)(xvb+k*xib) || (int)(dvar_y+k*dinc_y)!=(int)(yvb+k*yib)){
                        xvb = dvar_x;
                        yvb = dvar_y;
                        xib = dinc_x;
                        yib = dinc_y;
                        k=0;
                    }
                    else{
                        xvb = dvar_x;
                        yvb = dvar_y;
                        xib = dinc_x;
                        yib = dinc_y;

                        dvar_x += k*dinc_x;
                        dvar_y += k*dinc_y;
                        dvar_z += k*dinc_z;
                    }
                    while(k<dlim){
                        while(dvar_x<0){
                            dvar_x += MAP_WIDTH;
                        }
                        while(dvar_x>=MAP_WIDTH){
                            dvar_x -= MAP_WIDTH;
                        }
                        while(dvar_y<0){
                            dvar_y += MAP_HEIGHT;
                        }
                        while(dvar_y>=MAP_HEIGHT){
                            dvar_y -= MAP_HEIGHT;
                        }
                        xt = (int)(dvar_x);
                        yt = (int)(dvar_y);
                        t = yt*MAP_WIDTH + xt;
                        cptr = &cimgarr[0][0][0] + t*3;
                        hptr = &himgarr[0][0] + t;
                        if(dvar_z<=(*hptr)){
                            memcpy(fimg[i][j], cptr, 3);
                            break;
                        }
                        k++;
                        dvar_x += dinc_x;
                        dvar_y += dinc_y;
                        dvar_z += dinc_z;
                        if(dinc_z>=0 && dvar_z>256){
                            dvar_x += (dlim-k)*dinc_x;
                            dvar_y += (dlim-k)*dinc_y;
                            dvar_z += (dlim-k)*dinc_z;
                            k=dlim;
                        }
                    }
                    if(k==dlim){
                        memcpy(fimg[i][j], sky, 3);
                    }
                }
                dj -= p2ip;
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

    // hide mouse and capture all relative mouse motion unconstrained by window boundary
    SDL_SetRelativeMouseMode(SDL_TRUE);
    
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

    vanglimu = pb2 - atan(hl/d2s);
    vangliml = -vanglimu;

    // compute dbase
    dbase = sqrt(pow(wl, 2) + pow(d2s, 2));
    
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


void update(){
    while(SDL_PollEvent(&e)!=0){
        if(e.type == SDL_KEYDOWN){
            switch(e.key.keysym.sym){
                case SDLK_ESCAPE:
                quit = true;
                break;

                case SDLK_UP:
                fwd = true;
                break;

                case SDLK_DOWN:
                bwd = true;
                break;

                case SDLK_LEFT:
                lft = true;
                break;

                case SDLK_RIGHT:
                rht = true;
                break;
            }
        }
        else if(e.type == SDL_KEYUP){
            switch(e.key.keysym.sym){
                case SDLK_UP:
                fwd = false;
                break;

                case SDLK_DOWN:
                bwd = false;
                break;

                case SDLK_LEFT:
                lft = false;
                break;

                case SDLK_RIGHT:
                rht = false;
                break;
            }
        }
        else if(e.type == SDL_MOUSEMOTION){
            orih -= ang_spd*e.motion.xrel;
            oriv -= ang_spd*e.motion.yrel;
        }
    }

    if(oriv>vanglimu){
        oriv = vanglimu;
    }
    if(oriv<vangliml){
        oriv = vangliml;
    }

    if(fwd){
        x += spd*cos_oriv*cos_orih;
        y += spd*cos_oriv*sin_orih;
        z += spd*sin_oriv;
    }
    if(bwd){
        x -= spd*cos_oriv*cos_orih;
        y -= spd*cos_oriv*sin_orih;
        z -= spd*sin_oriv;
    }
    if(lft){
        x -= spd*sin_orih;
        y += spd*cos_orih;
    }
    if(rht){
        x += spd*sin_orih;
        y -= spd*cos_orih;
    }

    while(x<0){
        x += MAP_WIDTH;
    }
    while(x>=MAP_WIDTH){
        x -= MAP_WIDTH;
    }
    while(y<0){
        y += MAP_HEIGHT;
    }
    while(y>=MAP_HEIGHT){
        y -= MAP_HEIGHT;
    }
}


int main(){
    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::duration;
    using std::chrono::milliseconds;
    if(!my_setup()){
        return 0;
    }
    int i=0, j, ft=0;
    auto t1 = high_resolution_clock::now();
    auto t3=t1, t4=t1;
    while(!quit){
        t3 = high_resolution_clock::now();
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
        update();
        t4 = high_resolution_clock::now();
        ft = duration_cast<milliseconds>(t4-t3).count();
        if(ft<100){
            ft = 100-ft;
            SDL_Delay(ft);
        }
        i++;
    }
    auto t2 = high_resolution_clock::now();

    /* Getting number of milliseconds as a double. */
    duration<double, std::milli> du1 = (t2 - t1)/i;
    std::cout << du1.count() << "ms\n";

    my_clean();
    return 0;
}
