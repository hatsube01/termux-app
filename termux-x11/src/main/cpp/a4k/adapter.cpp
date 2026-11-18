#include "adapter.hpp"
#include "gl_state.hpp"
#include "color_order.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <vector>
namespace integrated {
extern void loadOptions();
static thread_local bool inside=false;
struct Quad {GLuint texture=0,program=0;GLint position=-1,uv=-1,sampler=-1,unit=0,view[4]{},blendSrc=GL_ONE,blendDst=GL_ZERO;float p[16]{},t[8]{};bool blend=false,bgra=false;int detectedOrder=0,tw=0,th=0;};
static bool attribute(int location,int first,int size,float* output){
 GLint enabled=0,type=0,n=0,stride=0,buffer=0;void* ptr=nullptr;
 glGetVertexAttribiv(location,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&enabled);glGetVertexAttribiv(location,GL_VERTEX_ATTRIB_ARRAY_TYPE,&type);
 glGetVertexAttribiv(location,GL_VERTEX_ATTRIB_ARRAY_SIZE,&n);glGetVertexAttribiv(location,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&stride);
 glGetVertexAttribiv(location,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&buffer);glGetVertexAttribPointerv(location,GL_VERTEX_ATTRIB_ARRAY_POINTER,&ptr);
 if(!enabled||type!=GL_FLOAT||n<2||n>4||first<0||first>1024)return false;
 if(!stride)stride=n*sizeof(float);if(stride<8||stride>4096)return false;
 const char* data=(const char*)ptr+size_t(first)*stride;GLint old=0;void* mapped=nullptr;
 if(buffer){
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&old);glBindBuffer(GL_ARRAY_BUFFER,buffer);GLint bytes=0;glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&bytes);
  size_t offset=uintptr_t(data),length=3*size_t(stride)+n*sizeof(float);
  if(offset>size_t(bytes)||length>size_t(bytes)-offset){glBindBuffer(GL_ARRAY_BUFFER,old);return false;}
  mapped=glMapBufferRange(GL_ARRAY_BUFFER,offset,length,GL_MAP_READ_BIT);data=(const char*)mapped;
 }
 bool ok=data!=nullptr;
 if(ok)for(int i=0;i<4;++i)for(int c=0;c<size;++c){float v=c==3?1.f:0.f;if(c<n)memcpy(&v,data+i*stride+c*sizeof(float),sizeof(float));output[i*size+c]=v;ok=ok&&std::isfinite(v);}
 if(mapped)glUnmapBuffer(GL_ARRAY_BUFFER);if(buffer)glBindBuffer(GL_ARRAY_BUFFER,old);return ok;
}
static bool readQuad(GLint first,Quad& q,std::string& code,std::string& detail){
 auto fail=[&](const char* why,const std::string& value){code=why;detail=value;return false;};
 GLint fbo=0,program=0,active=0;glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo);if(fbo)return fail("offscreen_framebuffer","draw framebuffer="+std::to_string(fbo));
 glGetIntegerv(GL_CURRENT_PROGRAM,&program);if(!program)return fail("no_program","GL_CURRENT_PROGRAM=0");q.program=program;
 q.position=glGetAttribLocation(program,"position");q.uv=glGetAttribLocation(program,"texCoords");GLint sampler=glGetUniformLocation(program,"texture");
 if(q.position<0)return fail("missing_position","program="+std::to_string(program)+" attribute position unavailable");
 if(q.uv<0)return fail("missing_texcoords","program="+std::to_string(program)+" attribute texCoords unavailable");
 if(sampler<0){GLint uniforms=0;glGetProgramiv(program,GL_ACTIVE_UNIFORMS,&uniforms);int matches=0;
  for(int i=0;i<uniforms;++i){GLchar name[256];GLsizei length=0;GLint size=0;GLenum type=0;glGetActiveUniform(program,i,sizeof(name),&length,&size,&type,name);if(type==GL_SAMPLER_2D&&size==1&&length<255){sampler=glGetUniformLocation(program,name);++matches;}}
  if(matches!=1)return fail("missing_sampler","program="+std::to_string(program)+" needs uniform texture or one unambiguous sampler2D");
 }q.sampler=sampler;
 glGetUniformiv(program,sampler,&q.unit);GLint maximum=0;glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,&maximum);
 if(q.unit<0||q.unit>=maximum)return fail("invalid_sampler_unit","unit="+std::to_string(q.unit)+" limit="+std::to_string(maximum));
 glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glActiveTexture(GL_TEXTURE0+q.unit);GLint texture=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);q.texture=texture;
 if(texture){glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&q.tw);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&q.th);}glActiveTexture(active);
 if(!texture||q.tw<1||q.th<1)return fail("missing_texture","unit="+std::to_string(q.unit)+" texture="+std::to_string(texture)+" size="+std::to_string(q.tw)+"x"+std::to_string(q.th));
 if(!attribute(q.position,first,4,q.p))return fail("invalid_position_array","position must be an enabled finite float array with a readable buffer");
 if(!attribute(q.uv,first,2,q.t))return fail("invalid_uv_array","texCoords must be an enabled finite float array with a readable buffer");
 bool known=false;GLuint shaders[8]{};GLsizei count=0;glGetAttachedShaders(program,8,&count,shaders);
 for(int i=0;i<count;++i){GLint type=0,length=0;glGetShaderiv(shaders[i],GL_SHADER_TYPE,&type);if(type!=GL_FRAGMENT_SHADER)continue;
  glGetShaderiv(shaders[i],GL_SHADER_SOURCE_LENGTH,&length);if(length<1||length>65536)continue;std::string source(length,'\0');glGetShaderSource(shaders[i],length,nullptr,source.data());std::string compact=compactShader(source);
  if(compact.find("texture2D(")!=std::string::npos||compact.find("texture(")!=std::string::npos){known=true;q.detectedOrder=shaderColorOrder(source);}
 }
 if(!known)return fail("unsupported_fragment_shader","copy fragment source must expose texture2D() or texture(); precompiled/custom shaders need an adapter");
 if(!q.detectedOrder&&colorOrder.load()==0)return fail("channel_order_unknown","fragment copy channel order could not be verified; choose RGBA/BGRA explicitly or provide an adapter");
 q.bgra=q.detectedOrder==2;
 glGetIntegerv(GL_VIEWPORT,q.view);q.blend=glIsEnabled(GL_BLEND);glGetIntegerv(GL_BLEND_SRC_RGB,&q.blendSrc);glGetIntegerv(GL_BLEND_DST_RGB,&q.blendDst);
 GLenum error=glGetError();return error==GL_NO_ERROR||fail("gl_inspection_error","GL error="+std::to_string(error));
}
struct Frame {
 EGLContext context=EGL_NO_CONTEXT;std::unique_ptr<a4k::Pipeline> pipeline;bool pending=false;int x=0,y=0,w=0,h=0,iw=0,ih=0;uint64_t serial=0;double prepareMs=0,captureMs=0;
 GLuint overlayVao=0,overlayVbo=0;a4k::Options capturedOptions{};std::vector<Quad> overlays;
 std::string rejectedCode,rejectedDetail,lastDiagnostic;int rejectedRank=0,draws=0,detectedOrder=0,usedOrder=0;double lastDiagnosticAt=-10000,capturePrepareMs=0;
};
static thread_local Frame frame;
static bool capable(){const char* s=(const char*)glGetString(GL_VERSION);int major=0,minor=0;return s&&sscanf(s,"OpenGL ES %d.%d",&major,&minor)==2&&(major>3||(major==3&&minor>=1));}
static std::string jsonEscape(const std::string& value){std::string s;for(unsigned char c:value){if(c=='"'||c=='\\'){s+='\\';s+=c;}else if(c=='\n')s+="\\n";else if(c>=32)s+=c;}return s;}
static void reject(const std::string& code,const std::string& detail,int rank){if(!frame.pending&&rank>=frame.rejectedRank){frame.rejectedCode=code;frame.rejectedDetail=detail;frame.rejectedRank=rank;}}
static void diagnose(){
 if(frame.rejectedCode.empty())reject("no_supported_draw","No capturable glDrawArrays quad in this presentation; glDrawElements/custom shaders are not automatically supported",1);
 std::string key=frame.rejectedCode;double at=a4k::Pipeline::now();
 if(!frame.lastDiagnostic.empty()&&at-frame.lastDiagnosticAt<1000)return;frame.lastDiagnostic=key;frame.lastDiagnosticAt=at;
 setReport("{\"ok\":false,\"active_mode\":0,\"requested_mode\":"+std::to_string(mode.load())+",\"stage\":\"capture\",\"capture_error_code\":\""+jsonEscape(frame.rejectedCode)+"\",\"error\":\""+jsonEscape(frame.rejectedDetail.substr(0,4000))+"\",\"draws_seen\":"+std::to_string(frame.draws)+",\"last_frame_available\":false,\"hooks\":"+hookStatus()+"}");
}
void draw(GLenum kind,GLint first,GLsizei count){
 auto original=originalDraw.load(std::memory_order_acquire);if(original)original(kind,first,count);else glDrawArrays(kind,first,count);if(inside)return;loadOptions();if(mode.load()==0)return;++frame.draws;
 if(kind!=GL_TRIANGLE_STRIP||count!=4){reject("unsupported_primitive","expected GL_TRIANGLE_STRIP count=4; got mode="+std::to_string(kind)+" count="+std::to_string(count),2);return;}
 if(!capable()){reject("gles_below_31","Original display context is below GLES 3.1; keep original display and reconnect/restart display after hooks are ready",9);return;}
 Quad q;std::string code,detail;if(!readQuad(first,q,code,detail)){reject(code,detail,code=="offscreen_framebuffer"?1:4);return;}
 if(q.tw<320||q.th<200){if(frame.pending&&frame.overlays.size()<8)frame.overlays.push_back(q);else reject("small_texture","only a small overlay/cursor texture was observed",1);return;}
 inside=true;{
  GLState saved;
  if(frame.context!=eglGetCurrentContext()){frame.pipeline.reset();frame.overlayVao=frame.overlayVbo=0;frame.context=eglGetCurrentContext();}
  if(!frame.pipeline)frame.pipeline.reset(new a4k::Pipeline);
  float xmin=1,xmax=-1,ymin=1,ymax=-1;int low=0,high=0;
  for(int i=0;i<4;++i){float x=q.p[i*4],y=q.p[i*4+1];if(x+y<q.p[low*4]+q.p[low*4+1])low=i;if(x+y>q.p[high*4]+q.p[high*4+1])high=i;xmin=std::min(xmin,x);xmax=std::max(xmax,x);ymin=std::min(ymin,y);ymax=std::max(ymax,y);}
  float crop[]={q.t[low*2],q.t[high*2+1],q.t[high*2],q.t[low*2+1]};
  int iw=int(lroundf(fabsf(crop[2]-crop[0])*q.tw)),ih=int(lroundf(fabsf(crop[3]-crop[1])*q.th));
  frame.x=q.view[0]+int(lroundf((xmin+1)*q.view[2]*.5f));frame.y=q.view[1]+int(lroundf((ymin+1)*q.view[3]*.5f));frame.w=int(lroundf((xmax-xmin)*q.view[2]*.5f));frame.h=int(lroundf((ymax-ymin)*q.view[3]*.5f));
  a4k::Options options{mode.load(),quality.load(),sharpness.load(),split.load()!=0,scale.load()};frame.detectedOrder=q.detectedOrder;int requested=colorOrder.load();frame.usedOrder=requested?requested:(q.detectedOrder?q.detectedOrder:1);
  frame.pipeline->setSourceFrame(++frame.serial);double prepAt=a4k::Pipeline::now();bool ready=iw>0&&ih>0&&frame.w>0&&frame.h>0&&frame.pipeline->prepareCapture(iw,ih);frame.capturePrepareMs=frame.prepareMs=a4k::Pipeline::now()-prepAt;
  double captureAt=a4k::Pipeline::now();frame.pending=ready&&frame.pipeline->capture(q.texture,crop,frame.usedOrder==2);frame.iw=iw;frame.ih=ih;frame.capturedOptions=options;frame.overlays.clear();
  // Do not defer this producer-ownership barrier without a proven release
  // protocol. Two private snapshots alone cannot protect a reused source.
  if(frame.pending)glFinish();frame.captureMs=a4k::Pipeline::now()-captureAt;
  if(!frame.pending){frame.pipeline->reportCaptureFailure(options,iw,ih,frame.w,frame.h);reject("snapshot_failed",frame.pipeline->json(),10);}else{frame.rejectedCode.clear();frame.rejectedDetail.clear();frame.rejectedRank=0;frame.lastDiagnostic.clear();}
 }inside=false;
}
static void replay(const Quad& q){
 glBindFramebuffer(GL_FRAMEBUFFER,0);if(!frame.overlayVao){glGenVertexArrays(1,&frame.overlayVao);glGenBuffers(1,&frame.overlayVbo);}
 glBindVertexArray(frame.overlayVao);glBindBuffer(GL_ARRAY_BUFFER,frame.overlayVbo);float vertices[24];memcpy(vertices,q.p,sizeof(q.p));memcpy(vertices+16,q.t,sizeof(q.t));glBufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STREAM_DRAW);glViewport(q.view[0],q.view[1],q.view[2],q.view[3]);glUseProgram(q.program);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,q.texture);glUniform1i(q.sampler,0);
 if(q.blend){glEnable(GL_BLEND);glBlendFunc(q.blendSrc,q.blendDst);}else glDisable(GL_BLEND);
 glEnableVertexAttribArray(q.position);glEnableVertexAttribArray(q.uv);glVertexAttribPointer(q.position,4,GL_FLOAT,GL_FALSE,0,nullptr);glVertexAttribPointer(q.uv,2,GL_FLOAT,GL_FALSE,0,(void*)(16*sizeof(float)));if(auto original=originalDraw.load(std::memory_order_acquire))original(GL_TRIANGLE_STRIP,0,4);glUniform1i(q.sampler,q.unit);
}
EGLBoolean swap(EGLDisplay display,EGLSurface surface){
 if(!inside)loadOptions();
 if(!inside&&frame.pending&&mode.load()>0&&frame.context==eglGetCurrentContext()){
  inside=true;{
   GLState saved;a4k::Options options{mode.load(),quality.load(),sharpness.load(),split.load()!=0,scale.load()};
   double prepAt=a4k::Pipeline::now();bool prepared=frame.pipeline->prepare(frame.iw,frame.ih,options);frame.prepareMs+=a4k::Pipeline::now()-prepAt;
   GLint maxTexture=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxTexture);
   double factor=options.scale/100.0;factor=std::min(factor,std::min(double(maxTexture)/frame.iw,double(maxTexture)/frame.ih));
   int outputW=std::max(1,int(lround(frame.iw*factor))),outputH=std::max(1,int(lround(frame.ih*factor)));
   if(!prepared)frame.pipeline->reportCaptureFailure(options,frame.iw,frame.ih,outputW,outputH);
   bool ok=prepared&&frame.pipeline->processCaptured(outputW,outputH,options);
   if(ok){frame.pipeline->present(frame.x,frame.y,frame.w,frame.h,options);for(const auto& q:frame.overlays)replay(q);}
   std::string s=frame.pipeline->json();if(!s.empty()){s.pop_back();s+=",\"stage\":\""+std::string(!prepared?"prepare":ok?"present":"process")+"\",\"prepare_ms\":"+std::to_string(frame.prepareMs)+",\"capture_prepare_ms\":"+std::to_string(frame.capturePrepareMs)+",\"capture_color_order\":"+std::to_string(frame.usedOrder)+",\"color_order_mode\":"+std::to_string(colorOrder.load())+",\"detected_color_order\":"+std::to_string(frame.detectedOrder)+",\"color_order_verified\":"+(frame.detectedOrder||colorOrder.load()?"true":"false")+",\"capture_sync\":\"glFinish_producer_ownership\",\"capture_ms\":"+std::to_string(frame.captureMs)+",\"display\":["+std::to_string(frame.w)+","+std::to_string(frame.h)+"],\"integrated\":true,\"source_path\":\"original_root_texture_snapshot\",\"wake_epoch\":"+std::to_string(wakeEpoch.load())+"}";setReport(s);}
  }inside=false;
 }else if(!inside&&mode.load()>0)diagnose();
 if(mode.load()==0&&frame.pipeline&&frame.context==eglGetCurrentContext()){GLState saved;frame.pipeline.reset();setReport("{\"ok\":true,\"active_mode\":0,\"note\":\"Original renderer; enhancement bypassed\"}");}
 frame.pending=false;frame.overlays.clear();frame.rejectedCode.clear();frame.rejectedDetail.clear();frame.rejectedRank=0;frame.draws=0;auto original=originalSwap.load(std::memory_order_acquire);return original?original(display,surface):eglSwapBuffers(display,surface);
}
}
