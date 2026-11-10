/* SPDX-License-Identifier: GPL-3.0-only
 * Hungry Travis: native, incremental 3D portrait for the 240 x 240 FM-1.
 * Include after gfx.c. No heap, libm, GPU, mesh cache, or platform I/O.
 * The sole large allocation is a 57,600-byte depth buffer in OM_POOL.
 * API: ht_face_draw(yaw_radians, pitch_radians, mouth_0_to_1, time_seconds).
 * The caller clears the canvas and draws its own controls after the portrait.
 */
#pragma once
#include "plat.h"
#include "../dsp/fastmath.h"

typedef struct { float x,y,z,l; } ht_vertex;
typedef struct { uint8_t r,g,b; } ht_material;
static uint8_t ht_depth[240u*240u] OM_POOL;
static float ht_cy,ht_sy,ht_cp,ht_sp,ht_breath,ht_expression;
static unsigned ht_triangles;
/* Ordered coverage is used only for dissolving the spoon, never the face. */
static int ht_coverage=16;
static float ht_eye_roll,ht_serenity;
/* Transient draw parameters, always reset each frame; never game/snapshot state. */
static float ht_model_scale=1,ht_model_x,ht_model_y,ht_model_z,ht_morph;
static int ht_portrait_warp,ht_features;
static void ht_model_identity(void){ht_model_scale=1;ht_model_x=ht_model_y=ht_model_z=0;ht_morph=0;ht_portrait_warp=ht_features=0;}
static const uint8_t ht_bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
static ht_material ht_skin={232,207,169},ht_lip={194,150,121},ht_fold={175,144,113},ht_nose={232,200,163};
static const ht_material ht_hair={35,29,25},ht_eye={254,250,226},ht_ink={31,30,25},ht_inside={69,29,24},ht_throat={30,16,15};
static void ht_face_set_anger(float amount){
 float a=fm_clampf(amount,0,1);ht_expression=a;
 ht_skin=(ht_material){(uint8_t)(232+10*a),(uint8_t)(207-112*a),(uint8_t)(169-88*a)};
 ht_nose=(ht_material){(uint8_t)(232+12*a),(uint8_t)(200-104*a),(uint8_t)(163-83*a)};
 ht_lip=(ht_material){(uint8_t)(194+10*a),(uint8_t)(150-66*a),(uint8_t)(121-46*a)};
 ht_fold=(ht_material){(uint8_t)(175+3*a),(uint8_t)(144-64*a),(uint8_t)(113-44*a)};
}

static ht_vertex ht_project(float x,float y,float z,float nx,float ny,float nz) {
    ht_vertex v;
    if(ht_portrait_warp&&ht_expression>0&&ht_morph<1){
      /* One smooth map for skin, eyes, lips and hair keeps surface layering intact.
       * Pin the delivery opening so visible mouth and physical catch stay aligned. */
      float a=ht_expression*(1-ht_morph),pin=fm_clampf((fm_fabsf(y+.56f)+fm_maxf(0,fm_fabsf(x)-.32f))/.65f,0,1);
      float oldx=x,oldy=y,side=oldx<0?-1.0f:1.0f;
      /* Broad carved cheekbones, hollow cheek planes and a heavy squared jaw:
       * a comic gargoyle grown from Travis's own face, with no added ornaments. */
      float cheek=fm_maxf(0,1-fm_fabsf(fm_fabsf(oldx)-.75f)*3.2f)*fm_maxf(0,1-fm_fabsf(oldy-.02f)*3.4f);
      float jaw=fm_maxf(0,1-fm_fabsf(oldy+.88f)*2.6f);
      float hollow=fm_maxf(0,1-fm_fabsf(fm_fabsf(oldx)-.65f)*3.0f)*fm_maxf(0,1-fm_fabsf(oldy+.40f)*4.0f);
      x+=a*pin*(side*(.19f*cheek+.20f*jaw)+.065f*fm_sinf(oldy*3.0f+.4f));
      y+=a*pin*(.055f*fm_sinf(oldx*2.8f)-.12f*jaw+.045f*cheek);
      z+=a*pin*(.15f*cheek-.12f*hollow+.06f*jaw);
    }
    if(ht_features&&ht_morph>0){float shrink=(1-ht_morph)*(1-ht_morph);x*=shrink;y=.1f+(y-.1f)*shrink;z=z*shrink+.8f*(1-shrink);}
    x=ht_model_x+x*ht_model_scale;y=ht_model_y+y*ht_model_scale;z=ht_model_z+z*ht_model_scale;
    float xx=ht_cy*x+ht_sy*z, zz=-ht_sy*x+ht_cy*z;
    float yy=ht_cp*y-ht_sp*zz;
    float nnx=ht_cy*nx+ht_sy*nz, nnz=-ht_sy*nx+ht_cy*nz;
    float nny=ht_cp*ny-ht_sp*nnz;
    float norm=fm_sqrtf(nx*nx+ny*ny+nz*nz);
    nnz=ht_sp*ny+ht_cp*nnz;
    v.x=120.0f+54.0f*xx;v.y=150.0f-54.0f*(yy+ht_breath);
    v.z=128.0f+60.0f*(ht_sp*y+ht_cp*zz);
    v.l=norm>.00001f ? .43f+.57f*fm_maxf(0.0f,(-.40f*nnx+.62f*nny+.675f*nnz)/norm) : .65f;
    return v;
}
static void ht_triangle(ht_vertex a,ht_vertex b,ht_vertex c,ht_material mat) {
    float area=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x),inv;
    float wa0,wb0,wc0,ax,ay,bx,by,cx,cy;int xmin,xmax,ymin,ymax,x,y;
    ht_triangles++;
    if(fm_fabsf(area)<.02f)return;
    xmin=(int)fm_floorf(fm_minf(a.x,fm_minf(b.x,c.x)));xmax=(int)fm_maxf(a.x,fm_maxf(b.x,c.x))+1;
    ymin=(int)fm_floorf(fm_minf(a.y,fm_minf(b.y,c.y)));ymax=(int)fm_maxf(a.y,fm_maxf(b.y,c.y))+1;
    if(xmin<0)xmin=0;if(xmax>239)xmax=239;if(ymin<0)ymin=0;if(ymax>239)ymax=239;
    inv=1.0f/area;
    ax=(b.y-c.y)*inv;ay=(c.x-b.x)*inv;
    bx=(c.y-a.y)*inv;by=(a.x-c.x)*inv;
    cx=(a.y-b.y)*inv;cy=(b.x-a.x)*inv;
    wa0=((b.x-(xmin+.5f))*(c.y-(ymin+.5f))-(b.y-(ymin+.5f))*(c.x-(xmin+.5f)))*inv;
    wb0=((c.x-(xmin+.5f))*(a.y-(ymin+.5f))-(c.y-(ymin+.5f))*(a.x-(xmin+.5f)))*inv;
    wc0=1.0f-wa0-wb0;
    for(y=ymin;y<=ymax;y++) {
        float wa=wa0,wb=wb0,wc=wc0;unsigned offset=(unsigned)y*240u+(unsigned)xmin;
        for(x=xmin;x<=xmax;x++,offset++) {
            if(wa>=-.0001f&&wb>=-.0001f&&wc>=-.0001f&&(ht_coverage>=16||ht_bayer[((y&3)<<2)|(x&3)]<ht_coverage)) {
                int depth=(int)(wa*a.z+wb*b.z+wc*c.z);
                if(depth>0&&depth>=ht_depth[offset]) {
                    float light=wa*a.l+wb*b.l+wc*c.l;
                    float dither=((float)ht_bayer[((y&3)<<2)|(x&3)]-7.5f)*.45f;
                    int r=(int)fm_clampf(mat.r*light+dither,0,255);
                    int g=(int)fm_clampf(mat.g*light+dither*.5f,0,255);
                    int bb=(int)fm_clampf(mat.b*light+dither,0,255);
                    ht_depth[offset]=(uint8_t)(depth>255?255:depth);
                    cv_px[offset]=swap16(RGB(r,g,bb));
                }
            }
            wa+=ax;wb+=bx;wc+=cx;
        }
        wa0+=ay;wb0+=by;wc0+=cy;
    }
}
static void ht_quad(ht_vertex a,ht_vertex b,ht_vertex c,ht_vertex d,ht_material mat) {
    ht_triangle(a,b,c,mat);ht_triangle(a,c,d,mat);
}
/* A broad forehead, fuller cheek band and tapered small chin. */
static float ht_head_radius(float y) {
    float t=(y-.10f)/1.45f;
    return 1.30f-.18f*t*t+.06f*t;
}
static float ht_face_z(float x,float y) {
    float rx=ht_head_radius(y),s=1.0f-x*x/(rx*rx)-(y-.10f)*(y-.10f)/(1.45f*1.45f);
    return .93f*fm_sqrtf(fm_maxf(0.0f,s));
}
static void ht_mouth_xy(float t,float opening,float *x,float *y) {
    float s=fm_sinf(t),c=fm_cosf(t);*x=(.31f+.11f*opening+.055f*ht_expression)*c;
    /* Dropped corners and a rounded lower lip preserve the stubborn pout. */
    *y=-.56f+(.15f*ht_serenity-.14f*ht_expression)*c*c+(.035f+.255f*opening)*s*(.72f+.28f*fm_fabsf(s));
}
static ht_vertex ht_head_point(float u,float t,float mouth,int back) {
    float x,y,z,mx,my,s=fm_sinf(t),c=fm_cosf(t);
    if(back) {y=.10f+1.45f*s*u;x=ht_head_radius(y)*c*u;z=-ht_face_z(x,y);}
    else {ht_mouth_xy(t,mouth,&mx,&my);y=my*(1-u)+(.10f+1.45f*s)*u;
      x=mx*(1-u)+ht_head_radius(.10f+1.45f*s)*c*u;z=ht_face_z(x,y);}
    if(ht_morph>0){
      /* The concentric front rings close the mouth into a complete food sphere. */
      float orb_x=1.18f*c*u,orb_y=.1f+1.18f*s*u,orb_z=(back?-1:1)*1.18f*fm_sqrtf(fm_maxf(0,1-u*u));
      x+=(orb_x-x)*ht_morph;y+=(orb_y-y)*ht_morph;z+=(orb_z-z)*ht_morph;
    }
    return ht_project(x,y,z,x*((1-ht_morph)/(1.30f*1.30f)+ht_morph/(1.18f*1.18f)),(y-.10f)*((1-ht_morph)/(1.45f*1.45f)+ht_morph/(1.18f*1.18f)),z*((1-ht_morph)/(.93f*.93f)+ht_morph/(1.18f*1.18f)));
}
static void ht_head(float mouth) {
    ht_vertex previous[33],current[33];int back,row,col;
    for(back=0;back<2;back++) {
        for(col=0;col<=32;col++)previous[col]=ht_head_point(0.0f,col*(FM_TWO_PI/32),mouth,back);
        for(row=1;row<=10;row++) {
            for(col=0;col<=32;col++)current[col]=ht_head_point(row/10.0f,col*(FM_TWO_PI/32),mouth,back);
            for(col=0;col<32;col++)ht_quad(previous[col],previous[col+1],current[col+1],current[col],ht_skin);
            for(col=0;col<=32;col++)previous[col]=current[col];
        }
    }
}
static ht_vertex ht_sphere_point(float u,float t,float x,float y,float z,float rx,float ry,float rz) {
    float s=fm_sinf(u),nx=s*fm_cosf(t),ny=fm_cosf(u),nz=s*fm_sinf(t);
    return ht_project(x+rx*nx,y+ry*ny,z+rz*nz,nx,ny,nz);
}
static void ht_sphere(float x,float y,float z,float rx,float ry,float rz,ht_material mat) {
    ht_vertex previous[13],current[13];int row,col;
    for(col=0;col<=12;col++)previous[col]=ht_sphere_point(0,col*FM_TWO_PI/12,x,y,z,rx,ry,rz);
    for(row=1;row<=6;row++) {
        for(col=0;col<=12;col++)current[col]=ht_sphere_point(row*FM_PI/6,col*FM_TWO_PI/12,x,y,z,rx,ry,rz);
        for(col=0;col<12;col++)ht_quad(previous[col],previous[col+1],current[col+1],current[col],mat);
        for(col=0;col<=12;col++)previous[col]=current[col];
    }
}
/* Smaller sphere mesh for the camera reveal; same shading/material as the
 * fully morphed head, 768 bounded triangles rather than another full portrait. */
static void ht_food_orb(void){
 ht_vertex previous[33],current[33];int row,col;
 for(col=0;col<=32;col++)previous[col]=ht_sphere_point(0,col*FM_TWO_PI/32,0,.1f,0,1.18f,1.18f,1.18f);
 for(row=1;row<=12;row++){
  for(col=0;col<=32;col++)current[col]=ht_sphere_point(row*FM_PI/12,col*FM_TWO_PI/32,0,.1f,0,1.18f,1.18f,1.18f);
  for(col=0;col<32;col++)ht_quad(previous[col],previous[col+1],current[col+1],current[col],ht_skin);
  for(col=0;col<=32;col++)previous[col]=current[col];
 }
}
static void ht_mouth(float opening) {
    int i,j;ht_vertex prev[7],next[7];float x,y,z,t,dx,dy,len;
    ht_vertex center=ht_project(0,-.60f,.38f,0,0,1);
    for(i=0;i<32;i++) {
        float t1=i*FM_TWO_PI/32,t2=(i+1)*FM_TWO_PI/32,x1,y1,x2,y2;
        ht_vertex a,b,c,d;
        ht_mouth_xy(t1,opening,&x1,&y1);ht_mouth_xy(t2,opening,&x2,&y2);
        a=ht_project(x1,y1,ht_face_z(x1,y1),0,0,1);b=ht_project(x2,y2,ht_face_z(x2,y2),0,0,1);
        c=ht_project(x2*.58f,-.60f+(y2+.60f)*.58f,.38f,0,0,1);d=ht_project(x1*.58f,-.60f+(y1+.60f)*.58f,.38f,0,0,1);
        ht_quad(a,b,c,d,ht_inside);ht_triangle(center,d,c,ht_throat);
    }
    for(i=0;i<=32;i++) {
        float radius;
        t=i*FM_TWO_PI/32;ht_mouth_xy(t,opening,&x,&y);z=ht_face_z(x,y)+.027f;
        radius=(.026f+.014f*fm_maxf(0,-fm_sinf(t)))*(1+.6f*ht_expression);
        {float xx,yy;ht_mouth_xy(t+.01f,opening,&xx,&yy);dx=xx-x;dy=yy-y;len=fm_sqrtf(dx*dx+dy*dy);if(len<.00001f)len=1;dx/=len;dy/=len;}
        for(j=0;j<=6;j++) {float q=j*FM_TWO_PI/6,c=fm_cosf(q),s=fm_sinf(q);next[j]=ht_project(x-radius*dy*c,y+radius*dx*c,z+radius*s,-dy*c,dx*c,s);}
        if(i)for(j=0;j<6;j++)ht_quad(prev[j],next[j],next[j+1],prev[j+1],ht_lip);
        for(j=0;j<=6;j++)prev[j]=next[j];
    }
}
static void ht_brow(float x) {
    int i,j;float side=x<0?-1.0f:1.0f;ht_vertex prev[7],next[7];
    for(i=0;i<=8;i++) {float u=i/8.0f,bx=x+(u-.5f)*(.42f+.055f*ht_expression);
        float by=.54f+.035f*ht_serenity-.05f*ht_expression+side*(u-.5f)*.19f*ht_expression+(.014f+.04f*ht_serenity)*fm_sinf(u*FM_PI);
        float bz=ht_face_z(bx,by)+.050f,r=.033f+.010f*fm_sinf(u*FM_PI)+.057f*ht_expression;
        for(j=0;j<=6;j++){float q=j*FM_TWO_PI/6,c=fm_cosf(q),s=fm_sinf(q);next[j]=ht_project(bx,by+r*c,bz+r*s,0,c,s);}
        if(i)for(j=0;j<6;j++)ht_quad(prev[j],prev[j+1],next[j+1],next[j],ht_ink);
        for(j=0;j<=6;j++)prev[j]=next[j];
    }
}
/* Small raised creases: softly shaded bags and forehead folds, not ink lines. */
static void ht_crease(float x,float y,float width,float curve) {
    int i,j;ht_vertex prev[5],next[5];
    for(i=0;i<=6;i++){float u=i/6.0f,bx=x+(u-.5f)*width,by=y+curve*fm_sinf(u*FM_PI);
        float bz=ht_face_z(bx,by)+.023f,r=.009f+.004f*fm_sinf(u*FM_PI)+.009f*ht_expression;
        for(j=0;j<=4;j++){float q=j*FM_TWO_PI/4,c=fm_cosf(q),s=fm_sinf(q);next[j]=ht_project(bx,by+r*c,bz+r*s,0,c,s);}
        if(i)for(j=0;j<4;j++)ht_quad(prev[j],prev[j+1],next[j+1],next[j],ht_fold);
        for(j=0;j<=4;j++)prev[j]=next[j];
    }
}
/* Seven cubic arcs retain the one distinctive swept curl in Adam's sketch. */
static const float ht_curl_arcs[7][6]={
 {-.74f,1.61f,-.62f,1.99f,-.36f,2.19f},{-.10f,2.40f,.28f,2.43f,.42f,2.19f},
 {.47f,2.10f,.45f,2.08f,.35f,2.10f},{.18f,2.16f,.05f,2.12f,.015f,2.015f},
 {-.015f,1.94f,.22f,1.965f,.30f,1.84f},{.43f,1.61f,.13f,1.46f,-.18f,1.48f},
 {-.31f,1.48f,-.43f,1.51f,-.49f,1.55f}};
static void ht_hair_draw(void) {
    float points[36][2],fx=-.49f,fy=1.55f;int arc,step,j,row,back,index=0;
    ht_vertex previous[36],current[36];
    for(arc=0;arc<7;arc++){const float *c=ht_curl_arcs[arc];
      for(step=0;step<5;step++){float t=step/5.0f,s=1-t;
        points[index][0]=s*s*s*fx+3*s*s*t*c[0]+3*s*t*t*c[2]+t*t*t*c[4];
        points[index++][1]=s*s*s*fy+3*s*s*t*c[1]+3*s*t*t*c[3]+t*t*t*c[5];}
      fx=c[4];fy=c[5];}
    points[35][0]=points[0][0];points[35][1]=points[0][1];
    for(back=0;back<2;back++)for(row=0;row<=4;row++){
      float r=row/4.0f,h=fm_sqrtf(fm_maxf(0,1-r*r)),depth=back?-.27f:.36f;
      for(j=0;j<=35;j++){
        float x=-.18f+(points[j][0]+.18f)*r,y=1.85f+(points[j][1]-1.85f)*r,z=.12f+depth*h;
        /* Seat the curl into the forehead rather than above the crown. */
        current[j]=ht_project(x,y-.16f,z,(x+.18f)*1.9f,(y-1.85f)*1.8f,back?-h:h);
      }
      if(row)for(j=0;j<35;j++)ht_quad(previous[j],previous[j+1],current[j+1],current[j],ht_hair);
      for(j=0;j<=35;j++)previous[j]=current[j];
    }
}
static void ht_face_draw(float yaw,float pitch,float mouth,float time) {
    unsigned i;int side;float blink=1.0f,phase;
    ht_cy=fm_cosf(yaw);ht_sy=fm_sinf(yaw);ht_cp=fm_cosf(pitch);ht_sp=fm_sinf(pitch);
    ht_breath=time>0?fm_sinf(time*1.3f)*.008f:0;ht_triangles=0;
    for(i=0;i<240u*240u;i++)ht_depth[i]=0;
    mouth=fm_clampf(mouth,0,1);ht_portrait_warp=1;ht_features=0;ht_head(mouth);ht_features=1;
    if(ht_morph>=1){ht_portrait_warp=ht_features=0;return;}
    ht_mouth(mouth);
    phase=time-fm_floorf(time/5.8f)*5.8f;
    if(time>0&&phase>4.85f&&phase<5.04f)blink=1.0f-.92f*fm_sinf((phase-4.85f)*FM_PI/.19f);
    for(side=-1;side<=1;side+=2) {
        float x=side*.49f,y=.26f,z=ht_face_z(x,y)+.044f;
        ht_sphere(x,y,z,.215f,(.165f-.035f*ht_expression)*(1-.75f*ht_serenity)*blink,.057f,ht_ink);
        ht_sphere(x,y-.008f,z+.030f,.190f,(.136f-.033f*ht_expression)*(1-.82f*ht_serenity)*blink,.085f,ht_eye);
        /* Pupils travel up the curved eye surface and tuck behind the upper lid. */
        float eye_angle=1.5f*ht_eye_roll;
        ht_sphere(x-side*.009f,y-.006f+.132f*(1-.82f*ht_serenity)*fm_sinf(eye_angle),z+.030f+.089f*fm_cosf(eye_angle),.043f,(.049f-.022f*ht_eye_roll)*(1-.82f*ht_serenity)*blink,.018f,ht_ink);
        ht_brow(x);
        ht_crease(x,.08f,.36f+.09f*ht_expression,-.035f-.045f*ht_expression);
    }
    ht_sphere(0,-.045f,ht_face_z(0,-.045f)+.045f,.135f+.055f*ht_expression,.105f+.025f*ht_expression,.145f+.035f*ht_expression,ht_nose);
    ht_crease(0,.78f,.40f,.022f);
    ht_crease(0,.89f,.29f,.014f);
    ht_hair_draw();ht_portrait_warp=ht_features=0;
}
