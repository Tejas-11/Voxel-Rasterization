#define MAP_WIDTH 1024
#define MAP_HEIGHT 1024
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720


__kernel void pixel_loop(__global double *x, __global double *y, __global double *z, __global const double *dbase, __global const double *dinc, __global const int *dlim, __global double *cos_orih, __global double *sin_orih, __global double *cos_oriv, __global double *sin_oriv, __global const double *cos_hvspan, __global const double *sin_hvspan, __global const double *cos_vvspan, __global const double *sin_vvspan, __global const double *himgarr, __global const uchar *cimgarr, __global uchar *fimg){
    int i = get_global_id(0);
    int j, k, xt, yt;

    double sin_theta, cos_theta, sin_phi, cos_phi;
    double op_buf, xinc, yinc, zinc, xvar, yvar, zvar;

    fimg += i*3;
    
    cos_hvspan += i%SCREEN_WIDTH;
    sin_hvspan += i%SCREEN_WIDTH;
    cos_vvspan += i;
    sin_vvspan += i;

    cos_theta = ((*cos_orih)*(*cos_hvspan))-((*sin_orih)*(*sin_hvspan));
    sin_theta = ((*cos_orih)*(*sin_hvspan))+((*sin_orih)*(*cos_hvspan));
    cos_phi = ((*cos_oriv)*(*cos_vvspan))-((*sin_oriv)*(*sin_vvspan));
    sin_phi = ((*cos_oriv)*(*sin_vvspan))+((*sin_oriv)*(*cos_vvspan));

    op_buf = cos_theta*cos_phi;
    xvar = *x + (*dbase)*op_buf;
    xinc = (*dinc)*op_buf;

    op_buf = sin_theta*cos_phi;
    yvar = *y + (*dbase)*op_buf;
    yinc = (*dinc)*op_buf;

    zvar = *z + (*dbase)*sin_phi;
    zinc = (*dinc)*sin_phi;

    fimg[0] = 135;
    fimg[1] = 206;
    fimg[2] = 235;
    
    for(k=0;k<(*dlim);k++){
        xt = ((int)floor(xvar)%MAP_WIDTH+MAP_WIDTH)%MAP_WIDTH;
        yt = ((int)floor(yvar)%MAP_HEIGHT+MAP_HEIGHT)%MAP_HEIGHT;
        j = yt*MAP_WIDTH+xt;
        if(himgarr[j]>=zvar){
            cimgarr += j*3;
            fimg[0] = cimgarr[0];
            fimg[1] = cimgarr[1];
            fimg[2] = cimgarr[2];
            break;
        }
        else{
            xvar += xinc;
            yvar += yinc;
            zvar += zinc;
        }
    }
}
