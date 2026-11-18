package com.a4k.integrated;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.graphics.Rect;
import android.graphics.Point;
import java.lang.reflect.Field;
import java.util.Arrays;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import android.widget.*;
import org.json.JSONObject;
import java.lang.reflect.Method;
public final class RenderMenuEntry extends LinearLayout {
 private final Handler handler=new Handler(Looper.getMainLooper());private String seenWake="";private boolean attached;
 private TextView stats;private Activity activity;private SharedPreferences prefs;
 private boolean dialogVisible,refreshBusy,refreshPending,refreshForced;
 private final ExecutorService refreshWorker=Executors.newSingleThreadExecutor(r->{Thread t=new Thread(r,"a4k-redraw");t.setDaemon(true);return t;});
 private final Runnable poll=new Runnable(){public void run(){if(!attached)return;
  String next=Bridge.getWake();if(!next.equals(seenWake)&&!controlsBusy()){seenWake=next;redraw();}
  if(refreshPending&&!controlsBusy())redraw();
  if(stats!=null)stats.setText(describe());handler.postDelayed(this,100);
 }};
 public RenderMenuEntry(Context c,AttributeSet a){super(c,a);setTag("a4k-render-controller");setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
  Context t=c;while(t instanceof ContextWrapper&&!(t instanceof Activity))t=((ContextWrapper)t).getBaseContext();if(t instanceof Activity)activity=(Activity)t;
  prefs=c.getSharedPreferences("a4k_integrated",0);Bridge.boot(c);readPreferences();
  update();
 }
 public static void show(Context context){
  Activity a=SettingsHeader.activity(context);if(a==null||a.isFinishing()||a.isDestroyed())return;
  RenderMenuEntry controller=a.getWindow().getDecorView().findViewWithTag("a4k-render-controller");
  if(controller==null){controller=new RenderMenuEntry(a,null);ViewGroup parent=a.findViewById(android.R.id.content);parent.addView(controller,new ViewGroup.LayoutParams(0,0));}
  if(!controller.dialogVisible)controller.open();
 }
 @Override protected void onAttachedToWindow(){super.onAttachedToWindow();attached=true;handler.post(()->{if(activity!=null)try{Object d=activity.getClass().getMethod("getDrawer").invoke(activity);if(d instanceof View)NativeDrawerEdge.install((View)d);}catch(Exception ignored){}});handler.post(poll);}
 @Override protected void onDetachedFromWindow(){attached=false;handler.removeCallbacks(poll);stats=null;super.onDetachedFromWindow();}
 private int dp(int x){return Math.round(x*getResources().getDisplayMetrics().density);}
 private TextView text(LinearLayout parent,String value){TextView t=new TextView(getContext());t.setText(value);t.setPadding(0,dp(6),0,dp(6));parent.addView(t);return t;}
 private int selected,quality,sharpness,scale,colorOrder;private boolean enabled,compare;
 private void update(){prefs.edit().putBoolean("enabled",enabled).putInt("mode",selected).putInt("quality",quality).putInt("scale",scale).putInt("color_order",colorOrder).putInt("profiles_schema",3).putInt("sharpness",sharpness).putBoolean("split",compare).apply();
  if(!Bridge.apply(enabled?selected:0,selected==3?quality:0,sharpness,compare,scale,colorOrder)){Bridge.boot(getContext());Bridge.apply(enabled?selected:0,selected==3?quality:0,sharpness,compare,scale,colorOrder);}redraw(true);
 }
 private boolean controlsBusy(){
  if(dialogVisible)return true;
  if(activity==null)return false;
  try{Object d=activity.getClass().getMethod("getDrawer").invoke(activity);
   Method m=d.getClass().getMethod("isDrawerVisible",int.class);
   return Boolean.TRUE.equals(m.invoke(d,0x800003))||Boolean.TRUE.equals(m.invoke(d,0x800005));
  }catch(Exception ignored){return false;}
 }
 private Object[] viewportSnapshot(){
  if(activity==null||activity.isFinishing()||!attached)return null;
  int id=getResources().getIdentifier("lorieView","id",getContext().getPackageName());View view=activity.findViewById(id);if(view==null)return null;
  try{Class<?> c=view.getClass();Field vf=null,pf=null;
   while(c!=null){try{vf=c.getDeclaredField("viewport");pf=c.getDeclaredField("p");break;}catch(NoSuchFieldException ignored){c=c.getSuperclass();}}
   if(vf==null||pf==null)return null;vf.setAccessible(true);pf.setAccessible(true);
   Rect r=(Rect)vf.get(view);Point p=(Point)pf.get(view);
   if(r.width()<=0||r.height()<=0||p.x<=0||p.y<=0)return null;
   Method m=c.getDeclaredMethod("setViewport",int.class,int.class,int.class,int.class,int.class,int.class);m.setAccessible(true);
   return new Object[]{m,new int[]{r.left,r.top,r.width(),r.height(),p.x,p.y}};
  }catch(Exception ignored){return null;}
 }
 private void redraw(){redraw(false);}
 private void redraw(boolean force){
  refreshPending=true;refreshForced|=force;if(refreshBusy||(!refreshForced&&controlsBusy()))return;
  Object[] snap=viewportSnapshot();if(snap==null)return;
  refreshPending=false;refreshForced=false;refreshBusy=true;int[] v=(int[])snap[1];Method m=(Method)snap[0];
  // Signal the existing renderer with unchanged geometry. No Java viewport,
  // pointer, focus or surface callbacks; no render-lock wait on the UI thread.
  refreshWorker.execute(()->{try{m.invoke(null,v[0],v[1],v[2],v[3],v[4],v[5]);}catch(Exception ignored){}
   handler.post(()->{refreshBusy=false;if(!attached)return;Object[] current=viewportSnapshot();
    if(current!=null&&!Arrays.equals(v,(int[])current[1]))refreshPending=true;
    if(refreshPending&&(refreshForced||!controlsBusy()))redraw();});
  });
 }
 private void readPreferences(){selected=Presets.mode(prefs.getInt("mode",7));quality=Math.max(0,Math.min(1,prefs.getInt("quality",0)));scale=Presets.scale(prefs.getInt("scale",150));colorOrder=Math.max(0,Math.min(2,prefs.getInt("color_order",0)));sharpness=Math.max(0,Math.min(100,prefs.getInt("sharpness",35)));enabled=prefs.getBoolean("enabled",false);compare=prefs.getBoolean("split",false);}
 private void open(){
  dialogVisible=true;
  readPreferences();
  LinearLayout content=new LinearLayout(getContext());content.setOrientation(LinearLayout.VERTICAL);content.setPadding(dp(18),dp(8),dp(18),dp(8));
  Switch master=new Switch(getContext());master.setText("启用画质增强");master.setChecked(enabled);master.setPadding(0,dp(6),0,dp(10));content.addView(master,new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,ViewGroup.LayoutParams.WRAP_CONTENT));
  master.setOnCheckedChangeListener((b,on)->{enabled=on;update();});
  text(content,"选择算法即时生效；关闭开关恢复原画。");
  Spinner algorithms=new Spinner(getContext());algorithms.setAdapter(new ArrayAdapter<String>(getContext(),android.R.layout.simple_spinner_dropdown_item,Presets.LABELS));algorithms.setSelection(Presets.index(selected));content.addView(algorithms);
  TextView note=text(content,"");
  Spinner tier=new Spinner(getContext());tier.setAdapter(new ArrayAdapter<String>(getContext(),android.R.layout.simple_spinner_dropdown_item,new String[]{"Anime4K 快速 S","Anime4K 质量 M"}));tier.setSelection(quality);content.addView(tier);
  text(content,"超分处理倍率（按真实输入计算，最终画面保持原窗口适配）");
  Spinner multiplier=new Spinner(getContext());multiplier.setAdapter(new ArrayAdapter<String>(getContext(),android.R.layout.simple_spinner_dropdown_item,new String[]{"1×","1.25×","1.5×","2×","2.5×","3×"}));
  for(int i=0;i<Presets.SCALES.length;i++)if(Presets.SCALES[i]==scale)multiplier.setSelection(i);content.addView(multiplier);
  text(content,"画面通道顺序（红蓝反转时可手动选择）");
  Spinner channels=new Spinner(getContext());channels.setAdapter(new ArrayAdapter<String>(getContext(),android.R.layout.simple_spinner_dropdown_item,new String[]{"自动（已知拷贝着色器）","RGBA","BGRA"}));channels.setSelection(colorOrder);content.addView(channels);
  channels.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){public void onNothingSelected(AdapterView<?> p){}public void onItemSelected(AdapterView<?> p,View v,int pos,long id){if(colorOrder!=pos){colorOrder=pos;update();}}});
  TextView strength=text(content,"CAS 强度："+sharpness+"%");SeekBar sharp=new SeekBar(getContext());sharp.setMax(100);sharp.setProgress(sharpness);content.addView(sharp);
  Runnable choices=()->{tier.setVisibility(selected==3?VISIBLE:GONE);sharp.setEnabled(selected!=8);
   note.setText(selected==6?"Lanczos2 缩放后接 CAS；速度优先是定位，设备上的实际耗时以状态为准。":selected==7?"AMD EASU 放大后接 CAS，仅锐化一次；1× 时只做 CAS。":selected==3?"官方 Anime4K S/M 内核原生 2×，再以 CAS 输出到所选尺寸。":"ArtCNN 仅使用 C4F16 轻量模型，GLES 计算；实验状态，首次编译和持续耗时需实测。内核原生 2×，所选倍率控制最终处理尺寸。");};choices.run();
  Switch splitView=new Switch(getContext());splitView.setText("同帧对比：左原画 / 右增强");splitView.setChecked(compare);content.addView(splitView);
  stats=text(content,describe());stats.setTextSize(12);stats.setTextIsSelectable(true);
  text(content,"1280×720 输入选择 1.5×，处理输出为 1920×1080；最终显示仍适配原窗口。DXVK / WineD3D 使用相同 X11 画面入口，两者真机兼容性与帧率均待验证。");
  algorithms.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){public void onNothingSelected(AdapterView<?> p){}public void onItemSelected(AdapterView<?> p,View v,int pos,long id){int next=Presets.IDS[pos];if(selected!=next){selected=next;choices.run();update();}}});
  tier.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){public void onNothingSelected(AdapterView<?> p){}public void onItemSelected(AdapterView<?> p,View v,int pos,long id){if(quality!=pos){quality=pos;if(selected==3)update();}}});
  multiplier.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener(){public void onNothingSelected(AdapterView<?> p){}public void onItemSelected(AdapterView<?> p,View v,int pos,long id){int next=Presets.SCALES[pos];if(scale!=next){scale=next;update();}}});
  sharp.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener(){public void onStartTrackingTouch(SeekBar s){}public void onStopTrackingTouch(SeekBar s){}public void onProgressChanged(SeekBar s,int n,boolean user){sharpness=n;strength.setText("CAS 强度："+n+"%");if(user)update();}});
  splitView.setOnCheckedChangeListener((b,on)->{compare=on;update();});
  ScrollView scroll=new ScrollView(getContext());scroll.addView(content);new AlertDialog.Builder(getContext()).setTitle("画质增强").setView(scroll).setPositiveButton("返回",null).setNeutralButton("重新检测",(d,w)->{Bridge.boot(getContext());redraw(true);}).setOnDismissListener(d->{stats=null;dialogVisible=false;redraw(true);}).show();
 }
 private String describe(){return "界面校验："+SettingsHeader.BUILD_MARK+"\n"+describeRenderer();}
 private String describeRenderer(){try{JSONObject j=new JSONObject(Bridge.getStatus());String error=Bridge.getError();if(!error.isEmpty())return "初始化未完成："+error;
  if(!j.optBoolean("ok",true))return "当前沿用原画 · "+j.optString("stage","初始化")+"\n"+j.optString("capture_error_code","")+" "+j.optString("error","等待兼容画面")+(j.has("hooks")?"\n接口检测："+j.optString("hooks"):"");
  int actual=j.optInt("active_mode",0);if(actual==0)return "原画 / 等待当前画面；开启后以实际处理状态为准。";
  return "实际："+j.optString("algorithm",Presets.label(actual))+"\n输入 "+j.optString("input")+" → 处理 "+j.optString("output")+"\n显示区域 "+j.optString("display")+" · 倍率 "+j.optDouble("processing_scale")+"×\nGPU "+(j.optDouble("gpu_ms",-1)>=0?j.optDouble("gpu_ms")+" ms":"不可用")+" · CPU提交 "+j.optDouble("cpu_submit_ms")+" ms\n捕获 "+j.optDouble("capture_ms")+" ms · 准备/编译 "+j.optDouble("prepare_ms")+" ms\n实际处理 Pass "+j.optInt("passes")+(j.optBoolean("render_reused")?"（复用当前画面）":"")+(actual==8?"\nArtCNN C4F16：原生 2× / 实验模式":"");
 }catch(Exception e){return "等待原显示引擎报告…";}}
}
