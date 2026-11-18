package com.a4k.integrated;

import android.view.View;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.WeakHashMap;

/** Adjust the existing END edge only. No touch listener or gesture recognizer. */
public final class NativeDrawerEdge {
 private static final WeakHashMap<View,Boolean> installed=new WeakHashMap<>();
 public static void install(View drawer){
  if(installed.containsKey(drawer))return;
  installed.put(drawer,true);apply(drawer);
  drawer.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or,ob)->apply(v));
 }
 private static void apply(View drawer){
  try{
   Class<?> c=drawer.getClass();Field f=null;
   String name=drawer.getLayoutDirection()==View.LAYOUT_DIRECTION_RTL?"mLeftDragger":"mRightDragger";
   while(c!=null){try{f=c.getDeclaredField(name);break;}catch(NoSuchFieldException absent){c=c.getSuperclass();}}
   if(f==null)return;f.setAccessible(true);Object dragger=f.get(drawer);if(dragger==null)return;
   Method get=dragger.getClass().getMethod("getEdgeSize");
   Method set=dragger.getClass().getMethod("setEdgeSize",int.class);
   int original=((Integer)get.invoke(dragger)).intValue();
   int requested=Math.round(24*drawer.getResources().getDisplayMetrics().density);
   if(original<requested)set.invoke(dragger,requested);
  }catch(Exception unavailable){/* Retain the original edge if unsupported. */}
 }
 private NativeDrawerEdge(){}
}
