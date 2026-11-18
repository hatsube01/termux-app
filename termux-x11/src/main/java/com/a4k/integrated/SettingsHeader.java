package com.a4k.integrated;

import android.app.Activity;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

/** Navigation confined to the original right settings container. No game gestures. */
public final class SettingsHeader extends LinearLayout {
 public static final String BUILD_MARK="分行返回栏";
 private static final java.util.WeakHashMap<Activity,String> screens=new java.util.WeakHashMap<>();
 private final TextView title;
 private final ImageButton back;
 public SettingsHeader(Context c,AttributeSet a){super(c,a);setTag("a4k-settings-header");setOrientation(VERTICAL);
  setBaselineAligned(false);setClipChildren(true);setClipToPadding(true);
  LinearLayout returnArea=new LinearLayout(c);returnArea.setOrientation(HORIZONTAL);returnArea.setGravity(Gravity.CENTER_VERTICAL);returnArea.setBaselineAligned(false);returnArea.setContentDescription("返回");
  TypedValue value=new TypedValue();if(c.getTheme().resolveAttribute(android.R.attr.selectableItemBackground,value,true))returnArea.setBackgroundResource(value.resourceId);
  TypedArray colors=c.obtainStyledAttributes(new int[]{android.R.attr.textColorPrimary});int color=colors.getColor(0,0xffdddddd);colors.recycle();
  back=new ImageButton(c);back.setBackground(null);back.setClickable(false);back.setFocusable(false);back.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
  back.setImageDrawable(new Arrow(color,dp(24)));back.setScaleType(ImageView.ScaleType.CENTER);back.setCropToPadding(true);back.setPadding(dp(12),dp(12),dp(12),dp(12));returnArea.addView(back,new LayoutParams(dp(48),dp(48)));
  TextView label=new TextView(c);label.setText("返回");label.setTextSize(16);label.setTextColor(color);label.setSingleLine(true);label.setIncludeFontPadding(false);label.setGravity(Gravity.CENTER_VERTICAL);label.setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);returnArea.addView(label,new LayoutParams(LayoutParams.WRAP_CONTENT,LayoutParams.MATCH_PARENT));
  returnArea.setPadding(0,0,dp(8),0);returnArea.setOnClickListener(v->navigateBack(getContext()));addView(returnArea,new LayoutParams(LayoutParams.MATCH_PARENT,dp(48)));
  title=new TextView(c);title.setText("设置");title.setTextSize(18);title.setTextColor(color);title.setSingleLine(true);title.setEllipsize(TextUtils.TruncateAt.END);title.setGravity(Gravity.START|Gravity.CENTER_VERTICAL);title.setIncludeFontPadding(false);title.setMinWidth(0);title.setPadding(dp(16),0,dp(16),0);
  addView(title,new LayoutParams(LayoutParams.MATCH_PARENT,dp(32)));setPadding(0,0,0,dp(8));
 }
 static Activity activity(Context c){while(c instanceof ContextWrapper&&!(c instanceof Activity)){Context next=((ContextWrapper)c).getBaseContext();if(next==c)break;c=next;}return c instanceof Activity?(Activity)c:null;}
 public static void update(Context c,CharSequence name,String key){Activity a=activity(c);if(a==null)return;screens.put(a,key==null?"main":key);
  SettingsHeader h=ensure(a);if(h!=null)h.title.setText(name==null||name.length()==0?"设置":name);
  bindOriginalToolbar(a,h!=null);
 }
 private static boolean integrated(Activity a){try{return a.getClass().getMethod("getTermuxActivityListener").invoke(a)!=null;}catch(Exception unavailable){return false;}}
 private static SettingsHeader ensure(Activity a){
  SettingsHeader h=a.getWindow().getDecorView().findViewWithTag("a4k-settings-header");
  if(h!=null){ViewGroup.LayoutParams p=h.getLayoutParams();if(p!=null&&p.height!=LayoutParams.WRAP_CONTENT){p.height=LayoutParams.WRAP_CONTENT;h.setLayoutParams(p);}return h;}
  if(!integrated(a))return null;
  int id=a.getResources().getIdentifier("id_window_preference","id",a.getPackageName());View container=a.findViewById(id);
  if(container==null||!(container.getParent() instanceof LinearLayout))return null;
  LinearLayout parent=(LinearLayout)container.getParent();parent.setOrientation(VERTICAL);
  LinearLayout.LayoutParams body=new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,0,1);container.setLayoutParams(body);
  h=new SettingsHeader(a,null);parent.addView(h,parent.indexOfChild(container),new LinearLayout.LayoutParams(LayoutParams.MATCH_PARENT,LayoutParams.WRAP_CONTENT));return h;
 }
 private static void bindOriginalToolbar(Activity a,boolean hasHeader){
  if(!integrated(a))return;
  // Also repair the original action-bar arrow shown by some base layouts.
  // The Home menu route is patched in LoriePreferences as a second entry.
  for(String name:new String[]{"action_bar","toolbar"}){
   int id=a.getResources().getIdentifier(name,"id",a.getPackageName());View toolbar=a.findViewById(id);if(toolbar==null)continue;
   try{toolbar.getClass().getMethod("setNavigationOnClickListener",View.OnClickListener.class).invoke(toolbar,(View.OnClickListener)v->navigateBack(a));if(hasHeader)toolbar.setVisibility(GONE);}catch(Exception ignored){}
  }
  if(hasHeader)try{Object bar=a.getClass().getMethod("getSupportActionBar").invoke(a);if(bar!=null)bar.getClass().getMethod("hide").invoke(bar);}catch(Exception ignored){}
 }
 public static void navigateBack(Context c){Activity a=activity(c);if(a==null||a.isFinishing()||a.isDestroyed())return;
  try{SettingsNavigation.back(a,screens.get(a));}catch(Exception failure){android.util.Log.e("A4KSettings","Settings navigation failed",failure);android.widget.Toast.makeText(a,"返回失败，请反馈此界面",android.widget.Toast.LENGTH_SHORT).show();}
 }
 private int dp(int n){return Math.round(n*getResources().getDisplayMetrics().density);}
 private static final class Arrow extends Drawable {
  private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
  private final int size;
  Arrow(int color,int size){this.size=size;paint.setColor(color);paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(2);paint.setStrokeCap(Paint.Cap.ROUND);paint.setStrokeJoin(Paint.Join.ROUND);}
  @Override public int getIntrinsicWidth(){return size;}
  @Override public int getIntrinsicHeight(){return size;}
  @Override public void draw(Canvas canvas){int saved=canvas.save();canvas.translate(getBounds().left,getBounds().top);canvas.scale(getBounds().width()/24f,getBounds().height()/24f);canvas.drawLine(20,12,4,12,paint);canvas.drawLine(4,12,11,5,paint);canvas.drawLine(4,12,11,19,paint);canvas.restoreToCount(saved);}
  @Override public void setAlpha(int alpha){paint.setAlpha(alpha);invalidateSelf();}
  @Override public void setColorFilter(ColorFilter filter){paint.setColorFilter(filter);invalidateSelf();}
  @Override public int getOpacity(){return PixelFormat.TRANSLUCENT;}
 }
}
