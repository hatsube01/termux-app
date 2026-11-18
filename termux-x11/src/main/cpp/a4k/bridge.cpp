#include "adapter.hpp"
#include "report_gate.hpp"
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <vector>
#include <chrono>
namespace integrated {
std::atomic<int> mode{0},quality{0},sharpness{35},split{0},scale{150},colorOrder{0};std::atomic<uint64_t> wakeEpoch{0};
static std::mutex io;static std::string current="{\"ok\":true,\"active_mode\":0,\"note\":\"Waiting for original X11 renderer\"}";
static constexpr const char* directory="/data/data/com.termux/files/a4k-renderer/";
static void writeFile(const char* name,const std::string& data){
 std::string path=std::string(directory)+name,temp=path+"."+std::to_string(getpid())+".new";
 FILE* f=fopen(temp.c_str(),"wb");if(!f)return;bool ok=fwrite(data.data(),1,data.size(),f)==data.size();fclose(f);if(ok)rename(temp.c_str(),path.c_str());else unlink(temp.c_str());
}
static std::string readFile(const char* name){char b[16385];FILE* f=fopen((std::string(directory)+name).c_str(),"rb");if(!f)return {};size_t n=fread(b,1,sizeof(b)-1,f);fclose(f);return std::string(b,n);}
void setReport(const std::string& s){
 std::lock_guard<std::mutex> lock(io);current=s;static ReportGate gate;if(gate.accept(s,a4k::Pipeline::now()))writeFile("status.json",s);
}
std::string report(){std::lock_guard<std::mutex> lock(io);auto saved=readFile("status.json");return saved.empty()?current:saved;}
void notify(){std::lock_guard<std::mutex> lock(io);auto epoch=++wakeEpoch;writeFile("wake",std::to_string(epoch)+" "+std::to_string(getpid()));}
void loadOptions(){
 static thread_local ino_t inode=0;static thread_local timespec modified{};struct stat st{};if(stat((std::string(directory)+"options.json").c_str(),&st)||(st.st_ino==inode&&st.st_mtim.tv_sec==modified.tv_sec&&st.st_mtim.tv_nsec==modified.tv_nsec))return;
 std::lock_guard<std::mutex> lock(io);int m,q,s,c,factor=150,order=0;auto text=readFile("options.json");
 if(sscanf(text.c_str(),"{\"mode\":%d,\"quality\":%d,\"sharpness\":%d,\"split\":%d,\"scale\":%d,\"color_order\":%d}",&m,&q,&s,&c,&factor,&order)>=4&&(m==0||m==3||m==6||m==7||m==8)&&q>=0&&q<=1&&s>=0&&s<=100&&(c==0||c==1)&&factor>=100&&factor<=300&&order>=0&&order<=2){
  mode=m;quality=q;sharpness=s;split=c;scale=factor;colorOrder=order;inode=st.st_ino;modified=st.st_mtim;
 }
}
}
extern "C" JNIEXPORT jint JNICALL Java_com_a4k_integrated_Bridge_install(JNIEnv*,jclass){return integrated::requestHooks();}
extern "C" JNIEXPORT void JNICALL Java_com_a4k_integrated_Bridge_configure(JNIEnv*,jclass,jint m,jint q,jint s,jboolean compare,jint factor,jint order){
 if(!(m==0||m==3||m==6||m==7||m==8)||q<0||q>1||s<0||s>100||factor<100||factor>300||order<0||order>2)return;
 {std::lock_guard<std::mutex> lock(integrated::io);char text[160];snprintf(text,sizeof(text),"{\"mode\":%d,\"quality\":%d,\"sharpness\":%d,\"split\":%d,\"scale\":%d,\"color_order\":%d}\n",m,q,s,compare?1:0,factor,order);integrated::writeFile("options.json",text);}
 integrated::mode=m;integrated::quality=q;integrated::sharpness=s;integrated::split=compare?1:0;integrated::scale=factor;integrated::colorOrder=order;
 if(!integrated::hooksReady())integrated::requestHooks();
 if(m==0)integrated::setReport("{\"ok\":true,\"active_mode\":0,\"note\":\"Original Lorie display; enhancement bypassed\"}");
 integrated::notify();
}
extern "C" JNIEXPORT jstring JNICALL Java_com_a4k_integrated_Bridge_status(JNIEnv* env,jclass){return env->NewStringUTF(integrated::report().c_str());}
extern "C" JNIEXPORT jstring JNICALL Java_com_a4k_integrated_Bridge_wake(JNIEnv* env,jclass){std::lock_guard<std::mutex> lock(integrated::io);auto s=integrated::readFile("wake");return env->NewStringUTF(s.c_str());}
