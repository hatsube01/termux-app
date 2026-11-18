#include "pipeline.hpp"
#include "generated_profiles.hpp"
#include "spatial_shaders.hpp"
#include "generated_shaders.hpp"
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <chrono>
#define A_CPU 1
#define A_GCC 1
#include "../../../../../../a4k/upstream/ffx_a.h"
#include "../../../../../../a4k/upstream/ffx_cas.h"
namespace a4k {
void easuConstants(uint32_t*,int,int,int,int);
static const char* vertex=R"GLSL(#version 310 es
precision highp float;
out vec2 uv;
void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));uv=p;gl_Position=vec4(p*2.0-1.0,0.0,1.0);}
)GLSL";
static const char* copySource=R"GLSL(#version 310 es
precision highp float;
in vec2 uv;out vec4 color;
uniform sampler2D tex;uniform vec4 crop;uniform bool bgra;
void main(){vec4 c=texture(tex,mix(crop.xy,crop.zw,uv));color=vec4(bgra?c.bgr:c.rgb,1.0);}
)GLSL";
static const char* displaySource=R"GLSL(#version 310 es
precision highp float;
in vec2 uv;out vec4 color;
uniform sampler2D tex,original;uniform bool split;uniform float outputWidth;
void main(){vec2 p=vec2(uv.x,1.0-uv.y);vec3 c=texture(tex,p).rgb;if(split&&uv.x<0.5)c=texture(original,p).rgb;if(split&&abs(uv.x-0.5)<1.0/outputWidth)c=vec3(0.2,0.9,1.0);color=vec4(c,1.0);}
)GLSL";
static void tightPixels(bool unpack){
 glBindBuffer(unpack?GL_PIXEL_UNPACK_BUFFER:GL_PIXEL_PACK_BUFFER,0);
 glPixelStorei(unpack?GL_UNPACK_ALIGNMENT:GL_PACK_ALIGNMENT,1);
 glPixelStorei(unpack?GL_UNPACK_ROW_LENGTH:GL_PACK_ROW_LENGTH,0);
 glPixelStorei(unpack?GL_UNPACK_SKIP_PIXELS:GL_PACK_SKIP_PIXELS,0);
 glPixelStorei(unpack?GL_UNPACK_SKIP_ROWS:GL_PACK_SKIP_ROWS,0);
}
static std::string escape(const std::string& s){std::string o;for(unsigned char c:s){if(c=='"'||c=='\\'){o+='\\';o+=c;}else if(c=='\n')o+="\\n";else if(c>=32)o+=c;}return o;}
Pipeline::Pipeline():ownerContext(eglGetCurrentContext()){}
double Pipeline::now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
GLuint Pipeline::program(const char* vs,const char* fs){GLuint shaders[2]={glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};const char* sources[]={vs,fs};for(int i=0;i<2;++i){glShaderSource(shaders[i],1,&sources[i],nullptr);glCompileShader(shaders[i]);GLint ok=0;glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&ok);if(!ok){char log[8192];glGetShaderInfoLog(shaders[i],sizeof(log),nullptr,log);error=log;for(auto s:shaders)glDeleteShader(s);return 0;}}GLuint p=glCreateProgram();for(auto s:shaders){glAttachShader(p,s);glDeleteShader(s);}glLinkProgram(p);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char log[8192];glGetProgramInfoLog(p,sizeof(log),nullptr,log);error=log;glDeleteProgram(p);return 0;}return p;}
bool Pipeline::init(){if(initialized)return copy&&cas&&display;if(!fbo){glGenFramebuffers(1,&fbo);glGenVertexArrays(1,&vao);}if(!copy)copy=program(vertex,copySource);cas=program(vertex,casSource);display=program(vertex,displaySource);initialized=true;first=now();
 const char* ext=reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));timer=ext&&strstr(ext,"GL_EXT_disjoint_timer_query");
 for(const auto* name:{"glGenQueriesEXT","glDeleteQueriesEXT","glBeginQueryEXT","glEndQueryEXT","glGetQueryObjectuivEXT","glGetQueryObjectui64vEXT"})if(!eglGetProcAddress(name))timer=false;
 if(timer){auto gen=reinterpret_cast<PFNGLGENQUERIESEXTPROC>(eglGetProcAddress("glGenQueriesEXT"));if(gen)gen(4,queries);else timer=false;}return copy&&cas&&display;}
void Pipeline::release(Tex& t){if(t.id)glDeleteTextures(1,&t.id);t=Tex{};}
Pipeline::~Pipeline(){if(ownerContext!=eglGetCurrentContext())return;
 if(hashProgram)glDeleteProgram(hashProgram);if(hashBuffer)glDeleteBuffers(1,&hashBuffer);
 for(auto p:fast)glDeleteProgram(p);for(auto p:quality)glDeleteProgram(p);for(auto p:artPrograms)glDeleteProgram(p);
 for(auto p:{copy,cas,casSharp,display,lanczos,easu,lumaProgram,artCompose})if(p)glDeleteProgram(p);
 for(auto* t:{&weightX,&weightY,&input,&baseline,&scaled,&result,&spatial,&luma,&head})release(*t);
 for(auto& t:cnn)release(t);for(auto& t:art)release(t);
 if(fbo)glDeleteFramebuffers(1,&fbo);if(vao)glDeleteVertexArrays(1,&vao);
 if(timer){auto del=reinterpret_cast<PFNGLDELETEQUERIESEXTPROC>(eglGetProcAddress("glDeleteQueriesEXT"));if(del)del(4,queries);}
}
bool Pipeline::alloc(Tex& t,int w,int h,bool floating){GLint max=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max);if(w<1||h<1||w>max||h>max){error="Texture size unsupported";return false;}if(t.id&&t.w==w&&t.h==h&&t.floating==floating)return true;if(t.id)glDeleteTextures(1,&t.id);glGenTextures(1,&t.id);t.w=w;t.h=h;t.floating=floating;glBindTexture(GL_TEXTURE_2D,t.id);tightPixels(true);glTexStorage2D(GL_TEXTURE_2D,1,floating?GL_RGBA16F:GL_RGBA8,w,h);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);return true;}
bool Pipeline::target(Tex& t){glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t.id,0);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){error=t.floating?"RGBA16F framebuffer unsupported; Anime4K unavailable":"Framebuffer incomplete";return false;}glViewport(0,0,t.w,t.h);return true;}
void Pipeline::bind(GLuint p,int unit,GLuint texture,const char* name){glActiveTexture(GL_TEXTURE0+unit);glBindTexture(GL_TEXTURE_2D,texture);glUniform1i(glGetUniformLocation(p,name),unit);}
void Pipeline::quad(){glBindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,3);}
bool Pipeline::prepareChain(bool high){auto& programs=high?quality:fast;const PassSource* ps=high?animeM:animeS;int count=high?int(sizeof(animeM)/sizeof(animeM[0])):int(sizeof(animeS)/sizeof(animeS[0]));if(programs.empty()){for(int i=0;i<count;++i){GLuint p=program(vertex,ps[i].code);if(!p){for(auto x:programs)glDeleteProgram(x);programs.clear();return false;}programs.push_back(p);}}return true;}
bool Pipeline::chain(bool high,GLuint& tex){if(!prepareChain(high))return false;auto& programs=high?quality:fast;const PassSource* ps=high?animeM:animeS;int count=high?int(sizeof(animeM)/sizeof(animeM[0])):int(sizeof(animeS)/sizeof(animeS[0]));
 for(int i=0;i<count;++i){int w=input.w*(ps[i].doubleSize?2:1),h=input.h*(ps[i].doubleSize?2:1);if(!alloc(cnn[i],w,h,i!=count-1)||!target(cnn[i]))return false;GLuint p=programs[i];glUseProgram(p);for(int b=0;b<ps[i].count;++b){int index=ps[i].binds[b];const Tex& t=index?cnn[index-1]:input;const char* ptr=ps[i].code;std::string prefix="uniform sampler2D t_";for(int j=0;j<=b;++j){ptr=strstr(ptr,prefix.c_str());if(!ptr){error="Generated binding mismatch";return false;}ptr+=prefix.size();}std::string name(ptr,strchr(ptr,';')-ptr);bind(p,b,t.id,("t_"+name).c_str());glUniform2f(glGetUniformLocation(p,("z_"+name).c_str()),t.w,t.h);}quad();}tex=cnn[count-1].id;return true;}
GLuint Pipeline::computeProgram(const char* source){
 GLuint s=glCreateShader(GL_COMPUTE_SHADER);glShaderSource(s,1,&source,nullptr);glCompileShader(s);GLint ok=0;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);
 if(!ok){char log[8192];glGetShaderInfoLog(s,sizeof(log),nullptr,log);error=log;glDeleteShader(s);return 0;}
 GLuint p=glCreateProgram();glAttachShader(p,s);glDeleteShader(s);glLinkProgram(p);glGetProgramiv(p,GL_LINK_STATUS,&ok);
 if(!ok){char log[8192];glGetProgramInfoLog(p,sizeof(log),nullptr,log);error=log;glDeleteProgram(p);return 0;}return p;
}
bool Pipeline::weights(Tex& t,int src,int dst,int& previous){
 if(t.id&&t.w==dst&&previous==src)return true;
 std::vector<float> values(size_t(dst)*8);
 auto sinc=[](double x){x=std::abs(x);if(x<1e-5)return 1.0;if(x>=2)return 0.0;double z=x*3.141592653589793;return std::sin(z)*std::sin(z*.5)/(z*z*.5);};
 for(int j=0;j<dst;++j){double p=(j+.5)*src/dst-.5,base=std::floor(p),total=0;for(int k=0;k<4;++k){double w=sinc(base+k-1-p);values[size_t(j)*4+k]=float(w);total+=w;}for(int k=0;k<4;++k)values[size_t(j)*4+k]/=float(total);values[size_t(dst+j)*4]=float(base);}
 release(t);glGenTextures(1,&t.id);t.w=dst;t.h=2;t.floating=true;glBindTexture(GL_TEXTURE_2D,t.id);tightPixels(true);
 glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32F,dst,2);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,dst,2,GL_RGBA,GL_FLOAT,values.data());
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);previous=src;return glGetError()==GL_NO_ERROR;
}
bool Pipeline::prepareArt(){
 GLint invocations=0,shared=0;glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,&invocations);glGetIntegerv(GL_MAX_COMPUTE_SHARED_MEMORY_SIZE,&shared);
 if(invocations<192||shared<16128){error="ArtCNN Lite needs 192 compute invocations and 16KB shared memory";return false;}
 if(!lumaProgram)lumaProgram=program(vertex,lumaSource);if(!artCompose)artCompose=program(vertex,artComposeSource);
 if(!lumaProgram||!artCompose)return false;
 if(artPrograms.empty())for(const auto& pass:artPasses){GLuint p=computeProgram(pass.code);if(!p){for(auto old:artPrograms)glDeleteProgram(old);artPrograms.clear();return false;}artPrograms.push_back(p);}
 return artPrograms.size()==7;
}
bool Pipeline::artChain(int ow,int oh,int& passes){
 int iw=input.w,ih=input.h;
 if(!alloc(luma,iw,ih,true)||!target(luma))return false;
 glUseProgram(lumaProgram);bind(lumaProgram,0,input.id,"tex");quad();++passes;
 if(!alloc(art[0],iw*2,ih*2,true)||!alloc(art[1],iw*2,ih*2,true)||!alloc(art[2],iw*2,ih*2,true)||!alloc(head,iw,ih,true))return false;
 auto layer=[&](int n)->Tex&{return n==0?art[0]:art[n%2?1:2];};
 for(int i=0;i<7;++i){auto& pass=artPasses[i];GLuint p=artPrograms[i];Tex& to=i==6?head:layer(i);
  // Detach any old FBO attachment before compute image writes.
  glBindFramebuffer(GL_FRAMEBUFFER,0);glUseProgram(p);
  for(int b=0;b<pass.count;++b){int from=pass.inputs[b];bind(p,b,from<0?luma.id:layer(from).id,b?"t1":"t0");}
  glUniform2i(glGetUniformLocation(p,"logicalSize"),iw,ih);
  glBindImageTexture(0,to.id,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);
  glDispatchCompute((iw+11)/12,(ih+15)/16,1);
  glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);++passes;
 }
 if(!alloc(scaled,ow,oh,false)||!target(scaled))return false;
 glUseProgram(artCompose);bind(artCompose,0,input.id,"tex");bind(artCompose,1,head.id,"packedTex");quad();++passes;finalTexture=scaled.id;
 return true;
}
void Pipeline::updateReport(const Options& o,int iw,int ih,int ow,int oh,int passes,double cpu,bool ok){
 ++frames;double elapsed=now()-first;const char* gpu=(const char*)glGetString(GL_RENDERER);
 const char* names[]={"Original","CAS-Scaling","Anime4K","Anime4K + CAS", "Removed", "Removed", "Lanczos2 + CAS", "FSR1 EASU + CAS", "ArtCNN Lite C4F16"};
 const char* name=o.mode>=0&&o.mode<=8?names[o.mode]:"Unsupported";
 std::ostringstream s;s<<"{\"ok\":"<<(ok?"true":"false")<<",\"requested_mode\":"<<o.mode<<",\"active_mode\":"<<(ok?o.mode:0)<<",\"algorithm\":\""<<name<<"\",\"quality\":\""<<((o.mode==2||o.mode==3)?(o.quality?"M":"S"):"fixed")<<"\",\"input\":["<<iw<<','<<ih<<"],\"output\":["<<ow<<','<<oh<<"],\"requested_scale\":"<<o.scale/100.0<<",\"processing_scale\":"<<(iw?double(ow)/iw:0)<<",\"passes\":"<<passes<<",\"cpu_submit_ms\":"<<cpu<<",\"gpu_ms\":"<<gpuMs<<",\"renderer\":\""<<escape(gpu?gpu:"")<<"\",\"error\":\""<<escape(error.substr(0,4000))<<"\",\"render_reused\":"<<(renderCached?"true":"false")<<",\"cas_scaling\":"<<((o.mode==1||o.mode==3)?"true":"false")<<",\"cas_sharpen_only\":"<<((o.mode==6||o.mode==7)?"true":"false")<<",\"cas_input\":["<<casInputW<<','<<casInputH<<"],\"cas_pre_resize\":"<<(casPreResize?"true":"false")<<",\"frames\":"<<frames<<",\"presents_per_s\":"<<(elapsed>0?frames*1000/elapsed:0)<<",\"timestamp_ms\":"<<now()<<",\"artcnn_experimental\":"<<(o.mode==8?"true":"false")<<",\"model_native_scale\":"<<((o.mode==2||o.mode==3||o.mode==8)?2:0)<<",\"artcnn_feature_bytes\":"<<(o.mode==8?uint64_t(iw)*ih*112:0)<<",\"shader_manifest\":"<<manifest<<",\"profile_manifest\":"<<profileManifest<<"}";
 report=s.str();
}
bool Pipeline::prepareHash(){
 if(!hashProgram){
  const char* text=R"HASH(#version 310 es
precision highp float;
precision highp int;
layout(local_size_x=16,local_size_y=16) in;
uniform sampler2D image;
layout(std430,binding=0) buffer Hashes {uvec2 sums[];};
shared uvec2 values[256];
uint mixbits(uint x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
void main(){
 uvec2 xy=gl_GlobalInvocationID.xy;ivec2 dims=textureSize(image,0);uint n=gl_LocalInvocationIndex;
 uvec2 value=uvec2(0);
 if(xy.x<uint(dims.x)&&xy.y<uint(dims.y)){
  uvec4 pixel=uvec4(round(texelFetch(image,ivec2(xy),0)*255.0));
  uint bits=pixel.r|(pixel.g<<8)|(pixel.b<<16)|(pixel.a<<24);
  uint pos=xy.y*uint(dims.x)+xy.x;
  value=uvec2(mixbits(bits^(pos*0x9e3779b9u)),mixbits(bits+(pos*0x85ebca6bu)));
 }
 values[n]=value;barrier();
 for(uint step=128u;step>0u;step>>=1u){if(n<step)values[n]^=values[n+step];barrier();}
 if(n==0u)sums[gl_WorkGroupID.y*gl_NumWorkGroups.x+gl_WorkGroupID.x]=values[0];
})HASH";
  GLuint shader=glCreateShader(GL_COMPUTE_SHADER);glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
  GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);if(!ok){glDeleteShader(shader);error="Content fingerprint compute shader unsupported";return false;}
  hashProgram=glCreateProgram();glAttachShader(hashProgram,shader);glLinkProgram(hashProgram);glDeleteShader(shader);
  glGetProgramiv(hashProgram,GL_LINK_STATUS,&ok);if(!ok){glDeleteProgram(hashProgram);hashProgram=0;error="Content fingerprint link failed";return false;}
 }
 if(!hashBuffer)glGenBuffers(1,&hashBuffer);
 return true;
}
bool Pipeline::contentHash(uint64_t& value){
 if(capturedRevision&&hashedRevision==capturedRevision){value=hashedValue;return true;}
 unsigned gx=(input.w+15)/16,gy=(input.h+15)/16;size_t bytes=size_t(gx)*gy*8;
 glBindBuffer(GL_SHADER_STORAGE_BUFFER,hashBuffer);
 if(hashBytes!=bytes){glBufferData(GL_SHADER_STORAGE_BUFFER,bytes,nullptr,GL_DYNAMIC_READ);hashBytes=bytes;}
 glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,hashBuffer);glUseProgram(hashProgram);bind(hashProgram,0,input.id,"image");
 glDispatchCompute(gx,gy,1);glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_BUFFER_UPDATE_BARRIER_BIT);
 auto* hashes=static_cast<const uint32_t*>(glMapBufferRange(GL_SHADER_STORAGE_BUFFER,0,bytes,GL_MAP_READ_BIT));
 if(!hashes){glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);return false;}
 value=1469598103934665603ull;for(size_t i=0;i<bytes/4;++i)value=(value^hashes[i])*1099511628211ull;
 bool ok=glUnmapBuffer(GL_SHADER_STORAGE_BUFFER)==GL_TRUE;glBindBuffer(GL_SHADER_STORAGE_BUFFER,0);
 if(ok&&glGetError()==GL_NO_ERROR){hashedRevision=capturedRevision;hashedValue=value;return true;}return false;
}
bool Pipeline::prepareCapture(int iw,int ih){
 error.clear();if(!fbo){glGenFramebuffers(1,&fbo);glGenVertexArrays(1,&vao);}if(!copy)copy=program(vertex,copySource);
 return copy&&alloc(input,iw,ih,false);
}
bool Pipeline::prepare(int iw,int ih,const Options& o){
 error.clear();if(!init()||!alloc(input,iw,ih,false))return false;
 if(o.mode==4||o.mode==5||o.mode<0||o.mode>8){error="Removed or unsupported rendering mode";return false;}
 if((o.mode==2||o.mode==3||o.mode==8)&&!prepareHash())return false;
 int keep=(o.mode==2||o.mode==3)?(o.quality?9:5):0;
 for(int i=keep;i<10;++i)release(cnn[i]);
 if(o.mode!=8){release(luma);release(head);for(auto& t:art)release(t);}
 if(o.mode!=6){release(spatial);release(weightX);release(weightY);weightSourceX=weightSourceY=0;}
 if(!o.split)release(baseline);
 if(o.mode==2||o.mode==3)return prepareChain(o.quality!=0);
 if(o.mode==6&&!lanczos)lanczos=program(vertex,lanczosSource);
 if(o.mode==7&&!easu)easu=program(vertex,easuSource);
 if((o.mode==6||o.mode==7)&&!casSharp){std::string text=casSource;auto at=text.rfind("const1,false)");if(at==std::string::npos){error="CAS entry missing";return false;}text.replace(at,13,"const1,true)");casSharp=program(vertex,text.c_str());}
 if(o.mode==8)return prepareArt();
 return (o.mode!=6||(lanczos&&casSharp))&&(o.mode!=7||(easu&&casSharp));
}
bool Pipeline::capture(GLuint source,const float crop[4],bool bgra){
 geometry=1469598103934665603ull;
 for(int i=0;i<4;++i){uint32_t bits;memcpy(&bits,crop+i,4);geometry=(geometry^bits)*1099511628211ull;}
 geometry=(geometry^uint64_t(input.w))*1099511628211ull;
 geometry=(geometry^uint64_t(input.h))*1099511628211ull;
 geometry=(geometry^uint64_t(bgra))*1099511628211ull;
 if(sourceFrame&&capturedSerial==sourceFrame&&capturedGeometry==geometry&&capturedSource==source)return true;
 if(!input.id||!copy||!target(input))return false;
 glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_DITHER);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glDisable(GL_RASTERIZER_DISCARD);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
 glUseProgram(copy);bind(copy,0,source,"tex");
 glUniform4fv(glGetUniformLocation(copy,"crop"),1,crop);
 glUniform1i(glGetUniformLocation(copy,"bgra"),bgra);quad();
 GLenum e=glGetError();if(e){error="Snapshot GLES error "+std::to_string(e);return false;}
 capturedSerial=sourceFrame;capturedGeometry=geometry;capturedSource=source;++capturedRevision;return true;
}
bool Pipeline::process(GLuint source,int iw,int ih,int ow,int oh,const float crop[4],bool bgra,const Options& o){
 if(!prepare(iw,ih,o)||!capture(source,crop,bgra)){
  updateReport(o,iw,ih,ow,oh,0,0,false);return false;
 }
 return processCaptured(ow,oh,o);
}
bool Pipeline::processCaptured(int ow,int oh,const Options& o){
 double begin=now();int passes=0,iw=input.w,ih=input.h;casInputW=casInputH=0;casPreResize=false;renderCached=false;
 if(!input.id||!copy||!cas||!display){updateReport(o,iw,ih,ow,oh,0,0,false);return false;}
 if(measuredMode!=o.mode||measuredW!=ow||measuredH!=oh){gpuMs=-1;measuredMode=o.mode;measuredW=ow;measuredH=oh;}
 error.clear();glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_DITHER);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glDisable(GL_RASTERIZER_DISCARD);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
 if(timer){auto get=reinterpret_cast<PFNGLGETQUERYOBJECTUIVEXTPROC>(eglGetProcAddress("glGetQueryObjectuivEXT"));auto get64=reinterpret_cast<PFNGLGETQUERYOBJECTUI64VEXTPROC>(eglGetProcAddress("glGetQueryObjectui64vEXT"));GLint disjoint=0;glGetIntegerv(GL_GPU_DISJOINT_EXT,&disjoint);for(int i=0;i<4;++i)if(pending[i]){GLuint ready=0;get(queries[i],GL_QUERY_RESULT_AVAILABLE_EXT,&ready);if(ready){GLuint64EXT n=0;get64(queries[i],GL_QUERY_RESULT_EXT,&n);if(queryMode[i]==o.mode&&queryW[i]==ow&&queryH[i]==oh)gpuMs=disjoint?-1:n/1e6;pending[i]=false;}}}
 bool timed=timer&&!pending[qi];if(timed)reinterpret_cast<PFNGLBEGINQUERYEXTPROC>(eglGetProcAddress("glBeginQueryEXT"))(GL_TIME_ELAPSED_EXT,queries[qi]);
 auto finish=[&](bool ok){if(timed){reinterpret_cast<PFNGLENDQUERYEXTPROC>(eglGetProcAddress("glEndQueryEXT"))(GL_TIME_ELAPSED_EXT);pending[qi]=true;queryMode[qi]=o.mode;queryW[qi]=ow;queryH[qi]=oh;qi=(qi+1)%4;}glBindFramebuffer(GL_FRAMEBUFFER,0);glActiveTexture(GL_TEXTURE0);updateReport(o,iw,ih,ow,oh,passes,now()-begin,ok);return ok;};
 uint64_t stamp=capturedRevision;
 if((o.mode==2||o.mode==3||o.mode==8)&&!contentHash(stamp))return finish(false);
 for(uint64_t part:{geometry,uint64_t(o.mode),uint64_t(o.quality),uint64_t(o.sharpness),uint64_t(o.split),uint64_t(ow),uint64_t(oh)})stamp=(stamp^part)*1099511628211ull;
 if(finalValid&&stamp==finalStamp){renderCached=true;casInputW=finalCasW;casInputH=finalCasH;casPreResize=finalCasResize;return finish(true);}
 finalValid=false;
 auto resize=[&](GLuint from,Tex& to,int w,int h){if(!alloc(to,w,h,false)||!target(to))return false;glUseProgram(copy);bind(copy,0,from,"tex");glUniform4f(glGetUniformLocation(copy,"crop"),0,0,1,1);glUniform1i(glGetUniformLocation(copy,"bgra"),0);quad();++passes;return true;};
 if(o.split&&!resize(input.id,baseline,ow,oh))return finish(false);
 GLuint working=input.id;int workW=iw,workH=ih;
 if(o.mode==2||o.mode==3){if(!chain(o.quality!=0,working))return finish(false);passes+=o.quality?9:5;workW=iw*2;workH=ih*2;}
 if(o.mode==6){
  bool cachedWeights=ow>=iw&&oh>=ih;if(cachedWeights&&(!weights(weightX,iw,ow,weightSourceX)||!weights(weightY,ih,oh,weightSourceY)))return finish(false);
  if(!alloc(spatial,ow,ih,true)||!target(spatial))return finish(false);
  glUseProgram(lanczos);bind(lanczos,0,input.id,"tex");bind(lanczos,1,cachedWeights?weightX.id:input.id,"weights");glUniform1i(glGetUniformLocation(lanczos,"useWeights"),cachedWeights);glUniform2f(glGetUniformLocation(lanczos,"dstSize"),ow,ih);glUniform2f(glGetUniformLocation(lanczos,"axis"),1,0);quad();++passes;
  if(!alloc(scaled,ow,oh,false)||!target(scaled))return finish(false);
  glUseProgram(lanczos);bind(lanczos,0,spatial.id,"tex");bind(lanczos,1,cachedWeights?weightY.id:spatial.id,"weights");glUniform1i(glGetUniformLocation(lanczos,"useWeights"),cachedWeights);glUniform2f(glGetUniformLocation(lanczos,"dstSize"),ow,oh);glUniform2f(glGetUniformLocation(lanczos,"axis"),0,1);quad();++passes;
  working=scaled.id;workW=ow;workH=oh;
 }
 if(o.mode==7&&(ow!=iw||oh!=ih)){
  if(ow<iw||oh<ih){error="FSR EASU is an upscaler; choose an output at least as large as input";return finish(false);}
  if(!alloc(scaled,ow,oh,false)||!target(scaled))return finish(false);
  glUseProgram(easu);bind(easu,0,input.id,"tex");uint32_t c[16];easuConstants(c,iw,ih,ow,oh);
  for(int j=0;j<4;++j){std::string n="const"+std::to_string(j);glUniform4uiv(glGetUniformLocation(easu,n.c_str()),1,c+j*4);}quad();++passes;
  working=scaled.id;workW=ow;workH=oh;
 }
 if(o.mode==8){if(!artChain(ow,oh,passes))return finish(false);}
 else if(o.mode==1||o.mode==3||o.mode==6||o.mode==7){
  bool sharpenOnly=o.mode==6||o.mode==7;
  if(!sharpenOnly&&!CasSupportScaling(float(ow),float(oh),float(workW),float(workH))){int pw=std::max(workW,(ow+1)/2),ph=std::max(workH,(oh+1)/2);if(!resize(working,scaled,pw,ph))return finish(false);working=scaled.id;workW=pw;workH=ph;casPreResize=true;}
  if(!alloc(result,ow,oh,false)||!target(result))return finish(false);
  GLuint p=sharpenOnly?casSharp:cas;glUseProgram(p);bind(p,0,working,"tex");uint32_t c0[4]{},c1[4]{};
  CasSetup(c0,c1,std::clamp(o.sharpness/100.f,0.f,1.f),float(workW),float(workH),float(ow),float(oh));
  glUniform4uiv(glGetUniformLocation(p,"const0"),1,c0);glUniform4uiv(glGetUniformLocation(p,"const1"),1,c1);quad();++passes;finalTexture=result.id;casInputW=workW;casInputH=workH;
 }else{if(!resize(working,scaled,ow,oh))return finish(false);finalTexture=scaled.id;}
 GLenum e=glGetError();if(e){error="GLES error "+std::to_string(e);return finish(false);}
 finalValid=true;finalStamp=stamp;finalCasW=casInputW;finalCasH=casInputH;finalCasResize=casPreResize;return finish(true);
}
void Pipeline::present(int x,int y,int w,int h,const Options& o){glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(x,y,w,h);glUseProgram(display);bind(display,0,finalTexture,"tex");bind(display,1,o.split?baseline.id:finalTexture,"original");glUniform1i(glGetUniformLocation(display,"split"),o.split);glUniform1f(glGetUniformLocation(display,"outputWidth"),w);quad();glActiveTexture(GL_TEXTURE0);glBindVertexArray(0);}
void Pipeline::presentCaptured(int x,int y,int w,int h){glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(x,y,w,h);glUseProgram(copy);bind(copy,0,input.id,"tex");glUniform4f(glGetUniformLocation(copy,"crop"),0,0,1,1);glUniform1i(glGetUniformLocation(copy,"bgra"),0);quad();glActiveTexture(GL_TEXTURE0);glBindVertexArray(0);}
bool Pipeline::readback(int w,int h,std::vector<uint8_t>& out){out.resize(size_t(w)*h*4);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,finalTexture,0);tightPixels(false);glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,out.data());glBindFramebuffer(GL_FRAMEBUFFER,0);return glGetError()==GL_NO_ERROR;}
// Independent scalar reference for CAS scaling (green-coefficient path, precise math).
static std::vector<uint8_t> referenceCasScaling(const std::vector<uint8_t>& in,int iw,int ih,int ow,int oh,float sharp){
 std::vector<uint8_t> out(size_t(ow)*oh*4,255);float peak=-1.f/(8.f-3.f*sharp);
 auto p=[&](int x,int y,int c){return in[(size_t(std::clamp(y,0,ih-1))*iw+std::clamp(x,0,iw-1))*4+c]/255.f;};
 for(int y=0;y<oh;++y)for(int x=0;x<ow;++x){
  float px=(x+.5f)*float(iw)/ow-.5f,py=(y+.5f)*float(ih)/oh-.5f;
  int ix=int(floorf(px)),iy=int(floorf(py));px-=floorf(px);py-=floorf(py);
  float weights[4],blend[4]={(1-px)*(1-py),px*(1-py),(1-px)*py,px*py};
  for(int j=0;j<4;++j){int cx=ix+j%2,cy=iy+j/2;
   float b=p(cx,cy-1,1),d=p(cx-1,cy,1),e=p(cx,cy,1),f=p(cx+1,cy,1),h=p(cx,cy+1,1);
   float mn=std::min({b,d,e,f,h}),mx=std::max({b,d,e,f,h});
   float amp=mx>0?std::clamp(std::min(mn,1.f-mx)/mx,0.f,1.f):0;
   weights[j]=sqrtf(amp)*peak;blend[j]/=(1.f/32.f+mx-mn);
  }
  float s=blend[0],t=blend[1],u=blend[2],v=blend[3];
  float be=weights[0]*s,ch=weights[1]*t,fn=weights[1]*t+weights[2]*u+s,
        gn=weights[0]*s+weights[3]*v+t,jn=weights[0]*s+weights[3]*v+u,
        kn=weights[1]*t+weights[2]*u+v,inw=weights[2]*u,lo=weights[3]*v;
  float norm=2*(be+ch+inw+lo)+fn+gn+jn+kn;
  for(int c=0;c<3;++c){
   float value=((p(ix,iy-1,c)+p(ix-1,iy,c))*be+(p(ix+1,iy-1,c)+p(ix+2,iy,c))*ch+
       (p(ix-1,iy+1,c)+p(ix,iy+2,c))*inw+(p(ix+2,iy+1,c)+p(ix+1,iy+2,c))*lo+
       p(ix,iy,c)*fn+p(ix+1,iy,c)*gn+p(ix,iy+1,c)*jn+p(ix+1,iy+1,c)*kn)/norm;
   out[(size_t(y)*ow+x)*4+c]=uint8_t(lroundf(std::clamp(value,0.f,1.f)*255));
  }
 }
 return out;
}
std::string Pipeline::selfTest(){if(!init())return "{\"pass\":false,\"error\":\""+escape(error)+"\"}";const int w=32,h=24,ow=64,oh=48;std::vector<uint8_t> pixels(w*h*4);for(int y=0;y<h;++y)for(int x=0;x<w;++x){int n=(y*w+x)*4;pixels[n]=(x*13+y*7)%256;pixels[n+1]=(x<16?60:130);pixels[n+2]=(y*19)%256;pixels[n+3]=255;}GLuint tex;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);float crop[]={0,0,1,1};Options opts;std::vector<uint8_t> base(ow*oh*4),actual(base.size());std::ostringstream log;bool all=true;for(int q=0;q<2;++q)for(int m=0;m<4;++m){opts.mode=m;opts.quality=q;bool ok=process(tex,w,h,ow,oh,crop,false,opts);if(ok){glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,finalTexture,0);glReadPixels(0,0,ow,oh,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());ok=glGetError()==GL_NO_ERROR;}if(m==0)base=actual;unsigned long long diff=0,checksum=1469598103934665603ull;for(size_t i=0;i<actual.size();++i){diff+=std::abs(int(actual[i])-int(base[i]));checksum=(checksum^actual[i])*1099511628211ull;}int maxError=0;if(m==1){auto ref=referenceCasScaling(pixels,w,h,ow,oh,opts.sharpness/100.f);for(size_t k=0;k<ref.size();++k)maxError=std::max(maxError,std::abs(int(ref[k])-int(actual[k])));ok=ok&&maxError<=2;}ok=ok&&(m==0||diff>0);all=all&&ok;if(q||m)log<<',';log<<"{\"mode\":"<<m<<",\"quality\":"<<q<<",\"pass\":"<<(ok?"true":"false")<<",\"cas_reference_max_error\":"<<maxError<<",\"difference_sum\":"<<diff<<",\"checksum\":\""<<checksum<<"\"}";}std::ostringstream scalingLog;
 const int dimensions[][2]={{32,24},{48,36},{24,18},{64,48},{96,72},{48,36},{48,36}};
 for(int test=0;test<7;++test){
  int tw=dimensions[test][0],th=dimensions[test][1];auto incoming=pixels;
  if(test>=5)for(size_t k=0;k<incoming.size();k+=4){incoming[k]=incoming[k+1]=incoming[k+2]=test==5?0:255;incoming[k+3]=255;}
  glBindTexture(GL_TEXTURE_2D,tex);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,incoming.data());
  Options scaleOpts;scaleOpts.mode=1;scaleOpts.sharpness=test%2?100:35;
  bool ok=process(tex,w,h,tw,th,crop,false,scaleOpts);std::vector<uint8_t> actualScale;
  ok=ok&&readback(tw,th,actualScale);int err=-1;
  if(ok&&!casPreResize){auto ref=referenceCasScaling(incoming,w,h,tw,th,scaleOpts.sharpness/100.f);err=0;
   for(size_t k=0;k<ref.size();++k)err=std::max(err,std::abs(int(ref[k])-int(actualScale[k])));ok=err<=2;}
  bool limitOk=CasSupportScaling(float(tw),float(th),float(casInputW),float(casInputH));
  ok=ok&&limitOk&&(casPreResize==(test==4));all=all&&ok;
  if(test)scalingLog<<',';scalingLog<<"{\"output\":["<<tw<<','<<th<<"],\"pass\":"<<(ok?"true":"false")<<",\"cpu_max_error\":"<<err<<",\"pre_resize\":"<<(casPreResize?"true":"false")<<"}";
 }
 glDeleteTextures(1,&tex);glBindFramebuffer(GL_FRAMEBUFFER,0);return std::string("{\"pass\":")+(all?"true":"false")+",\"cases\":["+log.str()+"],\"cas_scaling_cases\":["+scalingLog.str()+"],\"shader_manifest\":"+manifest+"}";}
}
