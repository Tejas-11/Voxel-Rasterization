#define MAP_WIDTH 1024
#define MAP_HEIGHT 1024
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720


__kernel void pixel_loop(__global double *x, __global double *y, __global double *z, __global const double *dbase, __global const double *dinc, __global const int *dlim, __global const double *d2s, __global double *cos_orih, __global double *sin_orih, __global double *cos_oriv, __global double *sin_oriv, __global const double *wl, __global const double *hl, __global const double *p2ip, __global const double *himgarr, __global const uchar *cimgarr, __global uchar *fimg){
    int j = get_global_id(0);
    int i, k;
    i = j/SCREEN_WIDTH;
    j = j%SCREEN_WIDTH;
    double di, dj, djs;
    int xt, yt;
    double t1, t2, t3, sin_hv, cos_hv;
    int t;

    double sin_theta, cos_theta, tan_phi;
    double xvar, xinc;
    double yvar, yinc;
    double zvar, zinc;
    
    double sin_ov = *sin_oriv;
    double cos_ov = *cos_oriv;
    double sin_oh = *sin_orih;
    double cos_oh = *cos_orih;
    double p2p = *p2ip;
    double dts = *d2s, din = *dinc, dba = *dbase;
    double bx = *x, by = *y, bz = *z;
    int dli = *dlim;
    
    fimg += j*3 + i*3*SCREEN_WIDTH;

    dj = *wl - j*p2p;
    djs = dj*dj;
    di = *hl - i*p2p;

    t1 = dts*cos_ov - di*sin_ov;
    t2 = t1*t1;
    t3 = sqrt(djs+t2);
    sin_hv = dj/t3;
    cos_hv = t1/t3;
    
    cos_theta = cos_oh*cos_hv-sin_oh*sin_hv;
    sin_theta = cos_oh*sin_hv+sin_oh*cos_hv;
    
    xvar = bx + dba*cos_theta;
    xinc = din*cos_theta;
    
    yvar = by + dba*sin_theta;
    yinc = din*sin_theta;
    
    tan_phi = (di*cos_ov+dts*sin_ov)/t3;
    
    zvar = bz + dba*tan_phi;
    zinc = din*tan_phi;

    k = 0;
    while(k<dli){
        xt = ((int)floor(xvar)%MAP_WIDTH+MAP_WIDTH)%MAP_WIDTH;
        yt = ((int)floor(yvar)%MAP_HEIGHT+MAP_HEIGHT)%MAP_HEIGHT;
        t = yt*MAP_WIDTH + xt;
        if(zvar<=(himgarr[t])){
            t = t*3;
            fimg[0] = cimgarr[t+0];
            fimg[1] = cimgarr[t+1];
            fimg[2] = cimgarr[t+2];
            break;
        }
        k++;
        xvar += xinc;
        yvar += yinc;
        zvar += zinc;
        if(zinc>=0 && zvar>256){
            k=dli;
        }
    }
    if(k==dli){
        fimg[0] = 135;
        fimg[1] = 206;
        fimg[2] = 235;
    }
}
