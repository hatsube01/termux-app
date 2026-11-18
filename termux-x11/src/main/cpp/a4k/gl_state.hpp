#pragma once
#include <GLES3/gl31.h>
#include <algorithm>
namespace integrated {
// The generated Anime4K ABI reserves eight input slots. ArtCNN C4F16 uses
// at most two. Force allocation onto unit zero so the caller's active unit
// outside this range can never be modified accidentally.
struct GLState {
 static constexpr int textureBudget=8;
 GLint program=0,vao=0,array=0,drawFbo=0,readFbo=0,viewport[4]{},scissor[4]{},active=0;
 GLint units=0,tex[textureBudget]{},sampler[textureBudget]{},pack=0,unpack=0,ssbo=0,ssbo0=0;
 GLint64 ssboStart=0,ssboSize=0;
 GLint pixels[8]{},blend[6]{},image0[6]{},frontFace=0,depthFunc=0,stencil[14]{};
 GLfloat depthRange[2]{},polygon[2]{};GLboolean colors[4]{},depthMask=0,enables[8]{};
 const GLenum flags[8]={GL_BLEND,GL_SCISSOR_TEST,GL_DEPTH_TEST,GL_STENCIL_TEST,GL_CULL_FACE,GL_RASTERIZER_DISCARD,GL_DITHER,GL_POLYGON_OFFSET_FILL};
 GLState(){
  glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&array);
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFbo);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFbo);glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_SCISSOR_BOX,scissor);
  glGetIntegerv(GL_ACTIVE_TEXTURE,&active);GLint maximum=0;glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,&maximum);units=std::min(maximum,textureBudget);
  for(int i=0;i<units;++i){glActiveTexture(GL_TEXTURE0+i);glGetIntegerv(GL_TEXTURE_BINDING_2D,tex+i);glGetIntegerv(GL_SAMPLER_BINDING,sampler+i);glBindSampler(i,0);}glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&pack);glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING,&unpack);
  const GLenum fields[]={GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_PIXELS,GL_PACK_SKIP_ROWS,GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS};
  for(int i=0;i<8;++i)glGetIntegerv(fields[i],pixels+i);
  glGetIntegerv(GL_SHADER_STORAGE_BUFFER_BINDING,&ssbo);glGetIntegeri_v(GL_SHADER_STORAGE_BUFFER_BINDING,0,&ssbo0);glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_START,0,&ssboStart);glGetInteger64i_v(GL_SHADER_STORAGE_BUFFER_SIZE,0,&ssboSize);
  const GLenum bf[]={GL_BLEND_SRC_RGB,GL_BLEND_DST_RGB,GL_BLEND_SRC_ALPHA,GL_BLEND_DST_ALPHA,GL_BLEND_EQUATION_RGB,GL_BLEND_EQUATION_ALPHA};for(int i=0;i<6;++i)glGetIntegerv(bf[i],blend+i);
  const GLenum im[]={GL_IMAGE_BINDING_NAME,GL_IMAGE_BINDING_LEVEL,GL_IMAGE_BINDING_LAYERED,GL_IMAGE_BINDING_LAYER,GL_IMAGE_BINDING_ACCESS,GL_IMAGE_BINDING_FORMAT};for(int i=0;i<6;++i)glGetIntegeri_v(im[i],0,image0+i);
  const GLenum sf[]={GL_STENCIL_FUNC,GL_STENCIL_REF,GL_STENCIL_VALUE_MASK,GL_STENCIL_FAIL,GL_STENCIL_PASS_DEPTH_FAIL,GL_STENCIL_PASS_DEPTH_PASS,GL_STENCIL_WRITEMASK,GL_STENCIL_BACK_FUNC,GL_STENCIL_BACK_REF,GL_STENCIL_BACK_VALUE_MASK,GL_STENCIL_BACK_FAIL,GL_STENCIL_BACK_PASS_DEPTH_FAIL,GL_STENCIL_BACK_PASS_DEPTH_PASS,GL_STENCIL_BACK_WRITEMASK};for(int i=0;i<14;++i)glGetIntegerv(sf[i],stencil+i);
  glGetBooleanv(GL_COLOR_WRITEMASK,colors);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);glGetFloatv(GL_DEPTH_RANGE,depthRange);glGetIntegerv(GL_FRONT_FACE,&frontFace);glGetFloatv(GL_POLYGON_OFFSET_FACTOR,polygon);glGetFloatv(GL_POLYGON_OFFSET_UNITS,polygon+1);
  for(int i=0;i<8;++i)enables[i]=glIsEnabled(flags[i]);glDisable(GL_POLYGON_OFFSET_FILL);
 }
 ~GLState(){
  glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,array);glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFbo);glBindFramebuffer(GL_READ_FRAMEBUFFER,readFbo);glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);glScissor(scissor[0],scissor[1],scissor[2],scissor[3]);
  for(int i=0;i<units;++i){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,tex[i]);glBindSampler(i,sampler[i]);}glActiveTexture(active);
  glBindBuffer(GL_PIXEL_PACK_BUFFER,pack);glBindBuffer(GL_PIXEL_UNPACK_BUFFER,unpack);
  const GLenum fields[]={GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_SKIP_PIXELS,GL_PACK_SKIP_ROWS,GL_UNPACK_ALIGNMENT,GL_UNPACK_ROW_LENGTH,GL_UNPACK_SKIP_PIXELS,GL_UNPACK_SKIP_ROWS};for(int i=0;i<8;++i)glPixelStorei(fields[i],pixels[i]);
  if(ssbo0&&ssboSize>0)glBindBufferRange(GL_SHADER_STORAGE_BUFFER,0,ssbo0,ssboStart,ssboSize);else glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,ssbo0);glBindBuffer(GL_SHADER_STORAGE_BUFFER,ssbo);
  glBlendFuncSeparate(blend[0],blend[1],blend[2],blend[3]);glBlendEquationSeparate(blend[4],blend[5]);glBindImageTexture(0,image0[0],image0[1],image0[2],image0[3],image0[4],image0[5]);
  glDepthMask(depthMask);glDepthFunc(depthFunc);glDepthRangef(depthRange[0],depthRange[1]);glFrontFace(frontFace);glPolygonOffset(polygon[0],polygon[1]);
  for(int f=0;f<2;++f){int n=f*7;GLenum face=f?GL_BACK:GL_FRONT;glStencilFuncSeparate(face,stencil[n],stencil[n+1],stencil[n+2]);glStencilOpSeparate(face,stencil[n+3],stencil[n+4],stencil[n+5]);glStencilMaskSeparate(face,stencil[n+6]);}
  glColorMask(colors[0],colors[1],colors[2],colors[3]);for(int i=0;i<8;++i)if(enables[i])glEnable(flags[i]);else glDisable(flags[i]);
 }
 GLState(const GLState&)=delete;GLState& operator=(const GLState&)=delete;
};
}
