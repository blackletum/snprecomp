#ifndef __SNP_ANDROID_H__
#define __SNP_ANDROID_H__

#if defined(__ANDROID__)
namespace snp::android {
    // Called first thing in main(): sends stdout/stderr to logcat (tag "SnP"), lets the renderer
    // find the game's surface again after the app was in the background, and makes the app's
    // private storage folder the working directory, which is where the activity has copied
    // assets/ and where relative paths such as crash_log.txt land.
    void startup();
}
#endif

#endif
