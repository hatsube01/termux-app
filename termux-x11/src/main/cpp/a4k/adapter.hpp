#pragma once
#include <GLES3/gl31.h>
#include <EGL/egl.h>
#include <atomic>
#include <string>
#include "pipeline.hpp"
namespace integrated {
extern std::atomic<int> mode,quality,sharpness,split,scale,colorOrder;
extern std::atomic<uint64_t> wakeEpoch;
using DrawFn=void(*)(GLenum,GLint,GLsizei);
using SwapFn=EGLBoolean(*)(EGLDisplay,EGLSurface);
extern std::atomic<DrawFn> originalDraw;
extern std::atomic<SwapFn> originalSwap;
void draw(GLenum,GLint,GLsizei);
EGLBoolean swap(EGLDisplay,EGLSurface);
int installHooks();
int requestHooks();
bool hooksReady();
std::string hookStatus();
void setReport(const std::string&);
void notify();
std::string report();
}
