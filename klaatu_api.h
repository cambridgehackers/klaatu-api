
#if defined(__cplusplus)
/* Version 1 interface definition */
class KlaatuAPITemplate_v1 {
public:
    const char *version(void) { return "1"; } /* KlaatuAPI interface version string */
    KlaatuAPITemplate_v1() {}
    int main(int argc, char *argv[]);
    void enable_accelerometer(void);
    void stop(void);
    virtual ~KlaatuAPITemplate_v1() {}

    /* these methods are overridden in the derived class for the application
     * (and invoked as event-driven callbacks)
     */
    virtual void init(int width, int height) { }
    virtual void draw(void) { }
    virtual void touchStart(float x, float y, unsigned int tap_count=0) { }
    virtual void touchMove(float x, float y, unsigned int tap_count=0) { }
    virtual void touchEnd(float x, float y, unsigned int tap_count=0) { }
    virtual void touchCancel(float x, float y, unsigned int tap_count=0) { }
    virtual void accel(float x, float y, float z) { }
    virtual void finish(void) { }
};
#endif /* defined(__cplusplus) */

/* for successive revisions of the api, just add the class definition
 * and change the following '#define'.
 */
#define KlaatuAPITemplate KlaatuAPITemplate_v1

/*
 * To use this api, at the beginning of the program, declare something like:
    #include "klaatu_api.h"
    class KlaatuAPI : public KlaatuAPITemplate {
    public:
        void init(int width, int height);
        void draw(void);
        void finish(void);
        // add templates as needed for the actual methods you implement
    };
    static KlaatuAPI current_methods;
 *
 * In the main() function, call:
    current_methods.main(argc, argv);
 *
 */

#if defined(__cplusplus)
extern "C" {
#endif
/*
 * These functions are called from support libraries like openal
 */
typedef void (*getdata_t)(void *device, void *buffer, int size);
void *KlaatuAudioOpen(void *arg_user, int arg_sampleRate, int arg_bytes, int arg_channels, int arg_frame_size, int arg_num_updates, getdata_t arg_get_data);
void KlaatuAudioStop(void *arg);
void KlaatuAudioDelete(void *arg);
#if defined(__cplusplus)
}
#endif
