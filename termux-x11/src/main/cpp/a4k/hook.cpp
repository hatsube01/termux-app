#include "adapter.hpp"
#include <link.h>
#include <elf.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>
namespace integrated {
std::atomic<DrawFn> originalDraw{nullptr};std::atomic<SwapFn> originalSwap{nullptr};
using CreateFn=EGLContext(*)(EGLDisplay,EGLConfig,EGLContext,const EGLint*);
static std::atomic<CreateFn> originalCreate{nullptr};
static std::mutex hookMutex;static std::atomic<int> attempts{0},installed{0};static std::atomic<bool> retrying{false},exhausted{false};
static bool targetFound=false,createInstalled=false;static std::string problem="not_attempted";
static EGLContext create(EGLDisplay d,EGLConfig cfg,EGLContext shared,const EGLint* attrs){
 auto real=originalCreate.load(std::memory_order_acquire);if(!real)return eglCreateContext(d,cfg,shared,attrs);
 EGLint changed[64];int i=0;bool upgrade=false;
 if(attrs)for(;i<60&&attrs[i]!=EGL_NONE;i+=2){changed[i]=attrs[i];changed[i+1]=attrs[i+1];if(attrs[i]==EGL_CONTEXT_CLIENT_VERSION&&attrs[i+1]==2){changed[i+1]=3;upgrade=true;}}
 if(upgrade){changed[i]=EGL_NONE;auto c=real(d,cfg,shared,changed);if(c!=EGL_NO_CONTEXT)return c;eglGetError();}return real(d,cfg,shared,attrs);
}
static int protection(uintptr_t p){
 FILE* f=fopen("/proc/self/maps","r");if(!f)return -1;char line[1024],perms[8];unsigned long a,b;int result=-1;
 while(fgets(line,sizeof(line),f))if(sscanf(line,"%lx-%lx %7s",&a,&b,perms)==3&&p>=a&&p<b){result=(perms[0]=='r'?PROT_READ:0)|(perms[1]=='w'?PROT_WRITE:0)|(perms[2]=='x'?PROT_EXEC:0);break;}fclose(f);return result;
}
template<class Fn> static bool replace(void** slot,Fn hook,std::atomic<Fn>& real){
 void* before=__atomic_load_n(slot,__ATOMIC_ACQUIRE);if(before==reinterpret_cast<void*>(hook))return real.load(std::memory_order_acquire)!=nullptr;
 int prot=protection(uintptr_t(slot));if(prot<0||!(prot&PROT_READ)||(prot&PROT_EXEC)){problem="got_permissions_unavailable";return false;}
 long pagesize=sysconf(_SC_PAGESIZE);if(pagesize<=0){problem="page_size_unavailable";return false;}auto page=uintptr_t(slot)&~uintptr_t(pagesize-1);
 if(mprotect(reinterpret_cast<void*>(page),pagesize,prot|PROT_WRITE)){problem="got_write_denied";return false;}
 real.store(reinterpret_cast<Fn>(before),std::memory_order_release);__atomic_store_n(slot,reinterpret_cast<void*>(hook),__ATOMIC_RELEASE);
 if(mprotect(reinterpret_cast<void*>(page),pagesize,prot)){__atomic_store_n(slot,before,__ATOMIC_RELEASE);mprotect(reinterpret_cast<void*>(page),pagesize,prot);problem="relro_restore_failed";return false;}return true;
}
int installHooks(){
 std::lock_guard<std::mutex> guard(hookMutex);++attempts;targetFound=false;createInstalled=false;problem="target_not_loaded";int found=0;
 dl_iterate_phdr(+[](dl_phdr_info* info,size_t,void* user){
  if(!info->dlpi_name)return 0;const char* name=strrchr(info->dlpi_name,'/');name=name?name+1:info->dlpi_name;if(strcmp(name,"libXlorie.so"))return 0;
  targetFound=true;problem="elf_dynamic_unavailable";uintptr_t base=info->dlpi_addr;const ElfW(Dyn)* dyn=nullptr;
  for(int i=0;i<info->dlpi_phnum;++i)if(info->dlpi_phdr[i].p_type==PT_DYNAMIC)dyn=(const ElfW(Dyn)*)(base+info->dlpi_phdr[i].p_vaddr);if(!dyn)return 0;
  auto ptr=[&](uintptr_t p){return p<base?p+base:p;};const char* strings=nullptr;const ElfW(Sym)* symbols=nullptr;const ElfW(Rela)* rel=nullptr;size_t bytes=0;long kind=0;
  for(;dyn->d_tag!=DT_NULL;++dyn)switch(dyn->d_tag){case DT_STRTAB:strings=(const char*)ptr(dyn->d_un.d_ptr);break;case DT_SYMTAB:symbols=(const ElfW(Sym)*)ptr(dyn->d_un.d_ptr);break;case DT_JMPREL:rel=(const ElfW(Rela)*)ptr(dyn->d_un.d_ptr);break;case DT_PLTRELSZ:bytes=dyn->d_un.d_val;break;case DT_PLTREL:kind=dyn->d_un.d_val;break;}
  problem="elf_jmprel_unavailable";if(!strings||!symbols||!rel||kind!=DT_RELA||bytes>4*1024*1024)return 0;
  bool drawReady=false,swapReady=false;problem="required_import_missing";
  for(size_t i=0;i<bytes/sizeof(*rel);++i){auto type=ELF64_R_TYPE(rel[i].r_info);
#if defined(__aarch64__)
   if(type!=R_AARCH64_JUMP_SLOT)continue;
#elif defined(__x86_64__)
   if(type!=R_X86_64_JUMP_SLOT)continue;
#endif
   const char* symbol=strings+symbols[ELF64_R_SYM(rel[i].r_info)].st_name;void** slot=(void**)(base+rel[i].r_offset);
   if(!strcmp(symbol,"glDrawArrays"))drawReady=replace(slot,draw,originalDraw);
   if(!strcmp(symbol,"eglSwapBuffers"))swapReady=replace(slot,swap,originalSwap);
   if(!strcmp(symbol,"eglCreateContext"))createInstalled=replace(slot,create,originalCreate);
  }
  *(int*)user=int(drawReady)+int(swapReady);if(drawReady&&swapReady)problem="";return 1;
 },&found);installed.store(found,std::memory_order_release);return found;
}
bool hooksReady(){return installed.load(std::memory_order_acquire)==2;}
std::string hookStatus(){std::lock_guard<std::mutex> guard(hookMutex);std::ostringstream s;s<<"{\"attempts\":"<<attempts.load()<<",\"installed\":"<<installed.load()<<",\"target_found\":"<<(targetFound?"true":"false")<<",\"create_context_hook\":"<<(createInstalled?"true":"false")<<",\"retrying\":"<<(retrying.load()?"true":"false")<<",\"timed_out\":"<<(exhausted.load()?"true":"false")<<",\"reason\":\""<<problem<<"\"}";return s.str();}
static void waiting(){setReport("{\"ok\":false,\"active_mode\":0,\"stage\":\"hook\",\"error\":\"Lorie hooks are not ready; inspect hooks.reason or retry detection\",\"hooks\":"+hookStatus()+"}");}
static void ready(){setReport("{\"ok\":true,\"active_mode\":0,\"stage\":\"waiting_frame\",\"note\":\"Lorie hooks installed; waiting for original source frame\",\"hooks\":"+hookStatus()+"}");notify();}
int requestHooks(){
 bool wasReady=hooksReady();int count=installHooks();if(count==2){exhausted=false;if(!wasReady)ready();return count;}
 bool expected=false;if(retrying.compare_exchange_strong(expected,true)){
  exhausted=false;try{std::thread([]{int delay=100;for(int i=0;i<18;++i){std::this_thread::sleep_for(std::chrono::milliseconds(delay));if(installHooks()==2){retrying=false;exhausted=false;ready();return;}waiting();delay=std::min(delay*2,2000);}exhausted=true;retrying=false;waiting();}).detach();}catch(...){retrying=false;exhausted=true;std::lock_guard<std::mutex> guard(hookMutex);problem="retry_worker_unavailable";}
 }waiting();return count;
}
}
