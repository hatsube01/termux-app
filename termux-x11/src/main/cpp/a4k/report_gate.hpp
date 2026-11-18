#pragma once
#include <string>
namespace integrated {
struct ReportGate {
 std::string previous;double last=-10000;
 static std::string token(const std::string& text,const char* key){std::string tag=std::string("\"")+key+"\":";auto p=text.find(tag);if(p==std::string::npos)return {};p+=tag.size();if(p<text.size()&&text[p]=='"'){auto e=text.find('"',p+1);return text.substr(p,e-p+1);}return text.substr(p,text.find_first_of(",}",p)-p);}
 bool accept(const std::string& text,double now){std::string key=token(text,"active_mode")+"|"+token(text,"ok")+"|"+token(text,"stage")+"|"+token(text,"capture_error_code");if(key!=previous||now-last>=400){previous=key;last=now;return true;}return false;}
};
}
