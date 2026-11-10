/* SPDX-License-Identifier: GPL-3.0-only
 * Hungry Thomas: native spoon and bite. Include after ht_face.h.
 * Shares the portrait projection, lighting conventions and existing depth buffer.
 * 496 submitted triangles; no heap or persistent geometry/frame buffers.
 * Local axes: X across the bowl, Y up, Z toward the viewer/handle.
 * Right-handed roll about Z, then pitch about X, then world translation.
 * Positive pitch exposes the bowl interior and lowers the handle on screen.
 */
#pragma once

typedef struct { float x,y,z; } ht_spoon_xyz;
typedef struct { float x,y,z,cr,sr,cp,sp; } ht_spoon_pose;
static const ht_material ht_spoon_steel={183,202,211},
 ht_spoon_under={136,154,163},ht_spoon_edge={221,231,233};
static ht_material ht_spoon_food={244,176,55};
static const ht_material ht_food_colors[4]={{244,176,55},{116,206,113},{233,130,157},{169,153,230}};
static void ht_spoon_set_food(int index){ht_spoon_food=ht_food_colors[(unsigned)index%4u];}

static ht_spoon_pose ht_spoon_pose_make(float x,float y,float z,float roll,float pitch) {
    ht_spoon_pose p;
    p.x=x;p.y=y;p.z=z;p.cr=fm_cosf(roll);p.sr=fm_sinf(roll);
    p.cp=fm_cosf(pitch);p.sp=fm_sinf(pitch);return p;
}
static ht_spoon_xyz ht_spoon_rotate(float x,float y,float z,const ht_spoon_pose *p) {
    ht_spoon_xyz v;float yy=p->sr*x+p->cr*y;
    v.x=p->cr*x-p->sr*y;v.y=p->cp*yy-p->sp*z;v.z=p->sp*yy+p->cp*z;
    return v;
}
/* Useful to collision/feeding code: this returns world coordinates, not pixels. */
static ht_spoon_xyz ht_spoon_transform(float localx,float localy,float localz,
 float x,float y,float z,float roll,float pitch) {
    ht_spoon_pose p=ht_spoon_pose_make(x,y,z,roll,pitch);
    ht_spoon_xyz v=ht_spoon_rotate(localx,localy,localz,&p);
    v.x+=x;v.y+=y;v.z+=z;return v;
}
static ht_vertex ht_spoon_point(float localx,float localy,float localz,
 float x,float y,float z,float roll,float pitch) {
    ht_spoon_pose p=ht_spoon_pose_make(x,y,z,roll,pitch);
    ht_spoon_xyz v=ht_spoon_rotate(localx,localy,localz,&p),n=ht_spoon_rotate(0,1,0,&p);
    return ht_project(v.x+x,v.y+y,v.z+z,n.x,n.y,n.z);
}
/* Interior floor height; a resting .13-radius bite sits this high plus .13. */
static float ht_spoon_floor(float x,float z) {
    return .11f*(x*x/(.32f*.32f)+z*z/(.36f*.36f));
}
static ht_vertex ht_spoon_surface(float x,float y,float z,float nx,float ny,float nz,
 const ht_spoon_pose *p,int metallic) {
    ht_spoon_xyz v=ht_spoon_rotate(x,y,z,p),n=ht_spoon_rotate(nx,ny,nz,p);
    ht_vertex out=ht_project(v.x+p->x,v.y+p->y,v.z+p->z,n.x,n.y,n.z);
    if(metallic) {
        float xx=ht_cy*n.x+ht_sy*n.z,zz=-ht_sy*n.x+ht_cy*n.z;
        float yy=ht_cp*n.y-ht_sp*zz,normal_z=ht_sp*n.y+ht_cp*zz;
        float norm=fm_sqrtf(n.x*n.x+n.y*n.y+n.z*n.z),spec;
        spec=norm>.00001f?fm_maxf(0,(-.218f*xx+.338f*yy+.916f*normal_z)/norm):0;
        spec*=spec;spec*=spec;spec*=spec;spec*=spec;
        out.l+=.70f*spec;
    }
    return out;
}
static void ht_spoon_bowl(const ht_spoon_pose *p) {
    ht_vertex prev[17],next[17];int side,row,i;
    for(side=0;side<2;side++)for(row=0;row<=2;row++) {
        float r=row*.5f,rx=side?.327f:.32f,rz=side?.367f:.36f;
        float y=side?-.035f+.145f*r*r:.11f*r*r;
        for(i=0;i<=16;i++) {
            float a=i*FM_TWO_PI/16,x=rx*r*fm_cosf(a),z=rz*r*fm_sinf(a);
            float nx=side?.29f*x/(rx*rx):-.22f*x/(rx*rx);
            float nz=side?.29f*z/(rz*rz):-.22f*z/(rz*rz);
            next[i]=ht_spoon_surface(x,y,z,nx,side?-1.0f:1.0f,nz,p,1);
        }
        if(row)for(i=0;i<16;i++)ht_quad(prev[i],prev[i+1],next[i+1],next[i],side?ht_spoon_under:ht_spoon_steel);
        for(i=0;i<=16;i++)prev[i]=next[i];
    }
    /* The rolled rim catches the highlight and makes the hollow bowl legible. */
    {
        ht_vertex a[5],b[5];int j;
        for(i=0;i<=16;i++) {
            float t=i*FM_TWO_PI/16,ct=fm_cosf(t),st=fm_sinf(t);
            for(j=0;j<=4;j++) {
                float q=j*FM_TWO_PI/4,c=fm_cosf(q),s=fm_sinf(q);
                b[j]=ht_spoon_surface((.323f+.018f*c)*ct,.11f+.018f*s,
                  (.363f+.018f*c)*st,ct*c,s,st*c,p,1);
            }
            if(i)for(j=0;j<4;j++)ht_quad(a[j],a[j+1],b[j+1],b[j],ht_spoon_edge);
            for(j=0;j<=4;j++)a[j]=b[j];
        }
    }
}
static void ht_spoon_handle(const ht_spoon_pose *p) {
    ht_vertex prev[7],next[7];int row,i;
    for(row=0;row<=8;row++) {
        float u=row/8.0f,z=.29f+.39f*u,y=.09f-.16f*u;
        float width=.040f+.025f*fm_sinf(u*FM_PI),thick=.019f;
        /* Rounded terminal cap; the root intersects the bowl naturally. */
        if(row==8){width=.006f;thick=.004f;}
        for(i=0;i<=6;i++) {
            float a=i*FM_TWO_PI/6,c=fm_cosf(a),s=fm_sinf(a);
            next[i]=ht_spoon_surface(width*c,y+thick*s,z,c,s,.18f*s,p,1);
        }
        if(row)for(i=0;i<6;i++)ht_quad(prev[i],prev[i+1],next[i+1],next[i],ht_spoon_edge);
        for(i=0;i<=6;i++)prev[i]=next[i];
    }
}
static void ht_spoon_bite(float x,float y,float z,const ht_spoon_pose *p) {
    ht_vertex previous[13],current[13];int row,i;
    for(row=0;row<=6;row++) {
        float a=row*FM_PI/6,sy=fm_sinf(a),cy=fm_cosf(a);
        for(i=0;i<=12;i++) {
            float t=i*FM_TWO_PI/12,c=fm_cosf(t),s=fm_sinf(t);
            float lump=1.0f+.075f*fm_sinf(3*t)*sy*sy+.035f*fm_cosf(5*t)*sy;
            current[i]=ht_spoon_surface(x+.13f*sy*c*lump,y+.13f*cy,
              z+.13f*sy*s*lump,sy*c,cy,sy*s,p,0);
        }
        if(row)for(i=0;i<12;i++)ht_quad(previous[i],previous[i+1],current[i+1],current[i],ht_spoon_food);
        for(i=0;i<=12;i++)previous[i]=current[i];
    }
}
static void ht_spoon_draw(float x,float y,float z,float roll,float pitch,
 float food_x,float food_z,float food_y,int food_visible) {
    ht_spoon_pose p=ht_spoon_pose_make(x,y,z,roll,pitch);
    ht_spoon_bowl(&p);ht_spoon_handle(&p);
    if(food_visible)ht_spoon_bite(food_x,food_y,food_z,&p);
}
