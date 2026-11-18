#pragma once
#include <string>
#include <cctype>
#include <algorithm>
namespace integrated {
// Conservative recognizer for Lorie's simple copy shaders. Complex shaders
// remain explicitly unknown; content statistics cannot determine channel order.
inline std::string compactShader(const std::string& source){
 std::string code;for(size_t i=0;i<source.size();){if(i+1<source.size()&&source[i]=='/'&&source[i+1]=='/'){i+=2;while(i<source.size()&&source[i]!='\n')++i;}
 else if(i+1<source.size()&&source[i]=='/'&&source[i+1]=='*'){i+=2;while(i+1<source.size()&&!(source[i]=='*'&&source[i+1]=='/'))++i;i=std::min(i+2,source.size());}
 else{if(!std::isspace(static_cast<unsigned char>(source[i]))&&source[i])code+=source[i];++i;}}return code;
}
inline int shaderColorOrder(const std::string& source){
 std::string s=compactShader(source);for(const char* function:{"texture2D(","texture("}){auto p=s.find(function);if(p==std::string::npos)continue;auto eq=s.rfind('=',p);if(eq==std::string::npos||eq+1!=p)continue;size_t lhs=eq;while(lhs&&((s[lhs-1]>='a'&&s[lhs-1]<='z')||(s[lhs-1]>='A'&&s[lhs-1]<='Z')||(s[lhs-1]>='0'&&s[lhs-1]<='9')||s[lhs-1]=='_'))--lhs;std::string output=s.substr(lhs,eq-lhs);if(output!="gl_FragColor"&&s.find("outvec4"+output+";")==std::string::npos)continue;int depth=1;size_t i=p+std::char_traits<char>::length(function);for(;i<s.size()&&depth;++i){if(s[i]=='(')++depth;else if(s[i]==')')--depth;}if(depth)continue;
  const auto tail=s.substr(i);if(tail.rfind(".bgra;",0)==0||tail.rfind(".bgr;",0)==0)return 2;if(tail.rfind(".rgba;",0)==0||tail.rfind(";",0)==0)return 1;
 }return 0;
}
}
