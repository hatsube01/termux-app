#pragma once
namespace a4k {
inline const char* lanczosSource=R"GLSL(#version 310 es
precision highp float;
in vec2 uv;out vec4 color;
uniform highp sampler2D tex,weights;uniform bool useWeights;uniform vec2 dstSize,axis;
float weight(float x){x=abs(x);if(x<0.00001)return 1.0;if(x>=2.0)return 0.0;float t=x*3.141592653589793;return sin(t)*sin(t*.5)/(t*t*.5);}
void main(){
 ivec2 dims=textureSize(tex,0);vec2 p=uv*vec2(dims)-.5;
 float pos=dot(p,axis),scale=max(dot(vec2(dims)/dstSize,axis),1.0),radius=2.0*scale;
 int base=int(floor(pos)),r=int(ceil(radius));vec3 sum=vec3(0.0);float total=0.0;
 if(useWeights){int index=int(axis.x>0.5?gl_FragCoord.x:gl_FragCoord.y);vec4 w=texelFetch(weights,ivec2(index,0),0);base=int(texelFetch(weights,ivec2(index,1),0).r);
  for(int k=0;k<4;++k){ivec2 q=ivec2(gl_FragCoord.xy);if(axis.x>0.5)q.x=base+k-1;else q.y=base+k-1;sum+=texelFetch(tex,clamp(q,ivec2(0),dims-1),0).rgb*w[k];}
  color=vec4(sum,1.0);return;
 }
 for(int k=1-r;k<=r;++k){float w=weight((float(base+k)-pos)/scale);ivec2 q=ivec2(floor(p+.5));if(axis.x>0.5)q.x=base+k;else q.y=base+k;sum+=texelFetch(tex,clamp(q,ivec2(0),dims-1),0).rgb*w;total+=w;}
 color=vec4(sum/total,1.0);
})GLSL";
inline const char* lumaSource=R"GLSL(#version 310 es
precision highp float;
in vec2 uv;out vec4 color;uniform sampler2D tex;
void main(){float y=dot(texture(tex,uv).rgb,vec3(.2126,.7152,.0722));color=vec4(y,0.0,0.0,1.0);}
)GLSL";
inline const char* artComposeSource=R"GLSL(#version 310 es
precision highp float;precision highp int;
in vec2 uv;out vec4 color;uniform sampler2D tex,packedTex;
float yAt(ivec2 p){p=clamp(p,ivec2(0),textureSize(packedTex,0)*2-1);vec4 v=texelFetch(packedTex,p/2,0);return clamp(v[(p.y%2)*2+(p.x%2)],0.0,1.0);}
void main(){vec2 p=uv*vec2(textureSize(packedTex,0)*2)-.5,f=fract(p);ivec2 q=ivec2(floor(p));
 float y=mix(mix(yAt(q),yAt(q+ivec2(1,0)),f.x),mix(yAt(q+ivec2(0,1)),yAt(q+ivec2(1,1)),f.x),f.y);
 vec3 rgb=texture(tex,uv).rgb;float oldY=dot(rgb,vec3(.2126,.7152,.0722));color=vec4(clamp(rgb+vec3(y-oldY),0.0,1.0),1.0);}
)GLSL";
}
