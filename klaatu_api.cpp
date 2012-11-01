
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <ui/DisplayInfo.h>
#include <ui/FramebufferNativeWindow.h>
#include <SurfaceComposerClient.h>

#include "gfx.h"
#include "klaatu_internal.h"

using namespace android;

#define ASSERT_EQ(A, B) {if ((A) != (B)) {printf ("ERROR: %d\n", __LINE__); exit(9); }}
#define ASSERT_NE(A, B) {if ((A) == (B)) {printf ("ERROR: %d\n", __LINE__); exit(9); }}
#define EXPECT_TRUE(A) {if ((A) == 0) {printf ("ERROR: %d\n", __LINE__); exit(9); }}

KlaatuAPITemplate *client;
int event_thread_stop;
size_t event_indication;
static EGLDisplay mEglDisplay = EGL_NO_DISPLAY;
static EGLSurface mEglSurface = EGL_NO_SURFACE;
static EGLContext mEglContext = EGL_NO_CONTEXT;
static sp<android::SurfaceComposerClient> mSession;
static sp<android::SurfaceControl>        mControl;
static sp<android::Surface>               mAndroidSurface;
static DisplayInfo display_info;
PROGRAM *gles_program = NULL;

static void test_exit(void)
{
    printf("test_exit...\n");
    if (gles_program && gles_program->vertex_shader) gles_program->vertex_shader = SHADER_free(gles_program->vertex_shader);
    if (gles_program && gles_program->fragment_shader) gles_program->fragment_shader = SHADER_free(gles_program->fragment_shader);
    if (gles_program) gles_program = PROGRAM_free(gles_program);
    if (mSession != NULL) mSession->dispose();
    if (mEglContext != EGL_NO_CONTEXT) eglDestroyContext(mEglDisplay, mEglContext);
    if (mEglSurface != EGL_NO_SURFACE) eglDestroySurface(mEglDisplay, mEglSurface);
    if (mEglDisplay != EGL_NO_DISPLAY) eglTerminate(mEglDisplay);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());
}

void initialize_graphics(int *width, int *height)
{
    static EGLint sDefaultContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    static EGLint sDefaultConfigAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 16, EGL_STENCIL_SIZE, 8, EGL_NONE };

    atexit(test_exit);
    mSession = new SurfaceComposerClient();
    int status = mSession->getDisplayInfo(0, &display_info);
    *width = display_info.w;
    *height = display_info.h;
    mControl = mSession->createSurface(
#if defined(SHORT_PLATFORM_VERSION) && (SHORT_PLATFORM_VERSION == 23)
         getpid(),
#endif /* on 2.3 */
         0, *width, *height, PIXEL_FORMAT_RGB_888);
    SurfaceComposerClient::openGlobalTransaction();
    mControl->setLayer(0x40000000);
    SurfaceComposerClient::closeGlobalTransaction();
    mAndroidSurface = mControl->getSurface();
    EGLNativeWindowType eglWindow = mAndroidSurface.get();

    mEglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());
    ASSERT_NE(EGL_NO_DISPLAY, mEglDisplay);
    EGLint majorVersion, minorVersion;
    EXPECT_TRUE(eglInitialize(mEglDisplay, &majorVersion, &minorVersion));
    ASSERT_EQ(EGL_SUCCESS, eglGetError());
    printf("EglVersion %d:%d\n", majorVersion, minorVersion);

    EGLint numConfigs = 0;
    EGLConfig  mGlConfig;
    EXPECT_TRUE(eglChooseConfig(mEglDisplay, sDefaultConfigAttribs, &mGlConfig, 1, &numConfigs));
    printf("numConfigs %d\n", numConfigs);
    mEglSurface = eglCreateWindowSurface(mEglDisplay, mGlConfig, eglWindow, NULL);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());
    ASSERT_NE(EGL_NO_SURFACE, mEglSurface);
    mEglContext = eglCreateContext(mEglDisplay, mGlConfig, EGL_NO_CONTEXT, sDefaultContextAttribs);
    ASSERT_EQ(EGL_SUCCESS, eglGetError());
    ASSERT_NE(EGL_NO_CONTEXT, mEglContext);
    EXPECT_TRUE(eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext));
    ASSERT_EQ(EGL_SUCCESS, eglGetError());
}

void KlaatuAPITemplate::stop(void)
{
    event_thread_stop = 1;
}
static int run_accel;
void KlaatuAPITemplate::enable_accelerometer(void)
{
    run_accel = 1;
}

#define MAX_FILENAME 1000
static char tempdir[MAX_FILENAME];

int KlaatuAPITemplate::main(int argc, char *argv[])
{
    int width, height;

    printf("high there\n");
    client = this;
    snprintf(tempdir, sizeof(tempdir), "%s.apk", argv[0]);
    initialize_graphics(&width, &height);
    setenv("FILESYSTEM", tempdir, 1);
    printf("before init\n");
    client->init(width, height);
    enable_touch(display_info.w, display_info.h);
    if (run_accel)
        start_accelerometer();
    while (!event_thread_stop) {
        client->draw();
        eglSwapBuffers(mEglDisplay, mEglSurface);
        if (event_indication)
            event_process();
    }
    printf("done\n");
    return 0;
}
