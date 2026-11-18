package com.a4k.integrated;
import android.content.Context;
import java.io.File;
public final class Bridge {
 private static boolean loaded;private static String error="";
 public static synchronized void boot(){boot(null);}
 public static synchronized void boot(Context context){
  try{
   String apk=context==null?System.getenv("CLASSPATH"):context.getApplicationInfo().sourceDir;
   if(apk==null||apk.isEmpty())apk=System.getProperty("java.class.path","");
   if(apk.contains(":")){String chosen="";for(String p:apk.split(":"))if(p.endsWith(".apk")&&new File(p).isFile()){chosen=p;break;}apk=chosen;}
   new File("/data/data/com.termux/files/a4k-renderer").mkdirs();
   if(!loaded){try{System.loadLibrary("A4KIntegrated");}catch(UnsatisfiedLinkError unavailable){if(!apk.endsWith(".apk"))throw unavailable;System.load(apk+"!/lib/arm64-v8a/libA4KIntegrated.so");}loaded=true;}
   install();
   error="";
  }catch(Throwable unavailable){error=unavailable.toString();}
 }
 public static String getStatus(){if(!loaded)return "{\"ok\":false,\"error\":\"Native renderer unavailable\"}";try{return status();}catch(Throwable e){return "{}";}}
 public static String getError(){return error;}
 public static String getWake(){try{return loaded?wake():"";}catch(Throwable e){return "";}}
 public static boolean apply(int mode,int quality,int sharpness,boolean split,int scale,int colorOrder){try{if(!loaded)return false;configure(mode,quality,sharpness,split,scale,colorOrder);return true;}catch(Throwable e){error=e.toString();return false;}}
 private static native int install();
 private static native void configure(int mode,int quality,int sharpness,boolean split,int scale,int colorOrder);
 private static native String status();private static native String wake();
 private Bridge(){}
}
