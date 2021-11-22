#define MAP_WIDTH 1024
#define MAP_HEIGHT 1024
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720


__kernel void pixel_loop(__global double *x, __global double *y, __global double *z, __global const double *dbase, __global const double *dinc, __global const int *dlim, __global double *cos_orih, __global double *sin_orih, __global double *tan_oriv, __global const double *cos_hvspan, __global const double *sin_hvspan, __global const double *tan_vvspan, __global const double *himgarr, __global const uchar *cimgarr, __global uchar *fimg){
    int j = get_global_id(0);
    int i, k;
    int xt, yt;
    int hb, he;
    int t, t3, fd = SCREEN_WIDTH*3;
    uchar *cptr;
    double *hptr;

    double sin_theta, cos_theta, tan_phi;
    double xinc, yinc, xvar, yvar;
    double zvar, zinc;

    fimg += j*3 + (SCREEN_HEIGHT-1)*fd;
    
    cos_hvspan += j;
    sin_hvspan += j;
    tan_vvspan += ((j+1)*SCREEN_HEIGHT) - 1;

    cos_theta = ((*cos_orih)*(*cos_hvspan))-((*sin_orih)*(*sin_hvspan));
    sin_theta = ((*cos_orih)*(*sin_hvspan))+((*sin_orih)*(*cos_hvspan));

    xvar = *x + (*dbase)*cos_theta;
    xinc = (*dinc)*cos_theta;

    yvar = *y + (*dbase)*sin_theta;
    yinc = (*dinc)*sin_theta;
    
    hb = 0;
    he = SCREEN_HEIGHT-1;
    i = he;
    k = 0;
    
    tan_phi = ((*tan_oriv)+(*tan_vvspan))/(1-((*tan_oriv)*(*tan_vvspan)));
    zvar = *z + (*dbase)*tan_phi;
    zinc = (*dinc)*tan_phi;
    tan_vvspan--;
    
    while(k<(*dlim)){
        xt = ((int)floor(xvar)%MAP_WIDTH+MAP_WIDTH)%MAP_WIDTH;
        yt = ((int)floor(yvar)%MAP_HEIGHT+MAP_HEIGHT)%MAP_HEIGHT;
        t = yt*MAP_WIDTH+xt;
        t3 = t*3;
        while(i>=hb && himgarr[t]>=(zvar+k*zinc)){
            fimg[0] = cimgarr[t3+0];
            fimg[1] = cimgarr[t3+1];
            fimg[2] = cimgarr[t3+2];
            i--;
            
            tan_phi = ((*tan_oriv)+(*tan_vvspan))/(1-((*tan_oriv)*(*tan_vvspan)));
            zvar = *z + (*dbase)*tan_phi;
            zinc = (*dinc)*tan_phi;
            tan_vvspan--;
            
            fimg -= fd;
        }
        if(i==-1){
            break;
        }
        k++;
        xvar += xinc;
        yvar += yinc;
    }
    if(k==(*dlim)){
        for(;i>=hb;i--){
            fimg[0] = 135;
            fimg[1] = 206;
            fimg[2] = 235;
            fimg -= fd;
        }
    }
}
