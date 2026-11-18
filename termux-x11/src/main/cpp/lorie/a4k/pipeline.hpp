#pragma once
#include <GLES3/gl31.h>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
namespace a4k {
struct Options {int mode=0,quality=0,sharpness=35;bool split=false;int scale=150;};
class Pipeline {
 struct Tex{GLuint id=0;int w=0,h=0;bool floating=false;};
 void* ownerContext=nullptr;
 GLuint fbo=0,vao=0,copy=0,cas=0,casSharp=0,display=0,lanczos=0,easu=0,lumaProgram=0,artCompose=0;
 GLuint hashProgram=0,hashBuffer=0;size_t hashBytes=0;
 std::vector<GLuint> fast,quality,artPrograms;
 Tex weightX,weightY;int weightSourceX=0,weightSourceY=0;
 Tex input,baseline,scaled,result,spatial,luma,head,art[3],cnn[10];
 GLuint queries[4]{};bool pending[4]{};int queryMode[4]{},queryW[4]{},queryH[4]{},measuredMode=-1,measuredW=0,measuredH=0;int qi=0;bool timer=false;double gpuMs=-1;
 bool initialized=false;unsigned long long frames=0;double first=0;GLuint finalTexture=0;
 int casInputW=0,casInputH=0,finalCasW=0,finalCasH=0;bool finalCasResize=false,casPreResize=false;
 std::string error,report;
 uint64_t sourceFrame=0,geometry=0,capturedSerial=0,capturedGeometry=0,capturedRevision=0,hashedRevision=0,hashedValue=0,finalStamp=0;
 GLuint capturedSource=0;bool finalValid=false,renderCached=false;
 bool init();GLuint program(const char*,const char*);GLuint computeProgram(const char*);
 bool alloc(Tex&,int,int,bool);bool target(Tex&);void bind(GLuint,int,GLuint,const char*);void quad();
 bool prepareChain(bool);bool chain(bool,GLuint&);bool prepareArt();bool artChain(int,int,int&);
 void updateReport(const Options&,int,int,int,int,int,double,bool);
 bool prepareHash();bool contentHash(uint64_t&);
 void release(Tex&);bool weights(Tex&,int,int,int&);
public:
 Pipeline();~Pipeline();
 void setSourceFrame(uint64_t serial){sourceFrame=serial;}
 bool prepareCapture(int,int);
 bool prepare(int,int,const Options&);
 bool capture(GLuint,const float[4],bool);
 bool processCaptured(int,int,const Options&);
 void presentCaptured(int,int,int,int);
 void reportCaptureFailure(const Options& o,int iw,int ih,int ow,int oh){updateReport(o,iw,ih,ow,oh,0,0,false);}
 bool process(GLuint,int,int,int,int,const float[4],bool,const Options&);
 void present(int,int,int,int,const Options&);const std::string& json() const{return report;}
 std::string selfTest();
 bool readback(int,int,std::vector<uint8_t>&);
 static double now();
};
}
