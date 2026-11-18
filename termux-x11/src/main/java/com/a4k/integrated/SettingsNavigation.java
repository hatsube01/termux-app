package com.a4k.integrated;

/** Use the original settings-return path, including its real sliding container. */
final class SettingsNavigation {
 static void back(Object activity,String screen) throws ReflectiveOperationException {
  // back2PreviousMenu() checks the LIVE stack and invokes the original back
  // dispatcher only above the first settings page. Never pop the root page.
  if(!"main".equals(screen)&&Boolean.TRUE.equals(activity.getClass().getMethod("back2PreviousMenu").invoke(activity)))return;
  // The right settings page is a DisplaySlidingWindow sibling of the game.
  // openPreference(false) updates its scroll position, state AND X11 focus.
  // getDrawer() refers to a different, terminal-side DrawerLayout.
  activity.getClass().getMethod("openPreference",boolean.class).invoke(activity,false);
 }
 private SettingsNavigation(){}
}
