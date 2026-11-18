package com.a4k.integrated;
public final class Presets {
 public static final int[] IDS={6,7,3,8};
 public static final int[] SCALES={100,125,150,200,250,300};
 public static final String[] LABELS={"Lanczos + CAS（速度优先）","FSR + CAS（均衡）","Anime4K + CAS（动漫）","ArtCNN Lite（实验）"};
 public static int mode(int old){if(old==1)return 6;if(old==2)return 3;if(old==4||old==5)return 7;for(int n:IDS)if(old==n)return n;return 7;}
 public static int index(int mode){for(int i=0;i<IDS.length;i++)if(IDS[i]==mode)return i;return 1;}
 public static int scale(int value){for(int n:SCALES)if(n==value)return n;return 150;}
 public static String label(int mode){return mode==0?"原画":LABELS[index(mode)];}
 private Presets(){}
}
