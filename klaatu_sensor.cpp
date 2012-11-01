
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <android/sensor.h>
#include <gui/Sensor.h>
#include <gui/SensorManager.h>
#include <gui/SensorEventQueue.h>
#include <utils/Looper.h>
#include "klaatu_internal.h"

using namespace android;

/* accelerometer */
static sp<SensorEventQueue> accelq;

static nsecs_t oldTimeStamp = 0;
int receiver(int fd, int events, void* data)
{
    ssize_t n;
static int jcac;
    ASensorEvent buffer[8];
    while ((n = accelq->read(buffer, 8)) > 0) {
        for (int i = 0 ; i < n ; i++) {
            float t = float(buffer[i].timestamp - oldTimeStamp) / s2ns(1);
            oldTimeStamp = buffer[i].timestamp;
            if (buffer[i].type == Sensor::TYPE_ACCELEROMETER) {
//if (jcac++ % 100 == 0)
//                printf("%lld\t%8f\t%8f\t%8f\t%f\n", buffer[i].timestamp, buffer[i].data[0], buffer[i].data[1], buffer[i].data[2], 1.0/t);
            }
        }
    }
    if (n<0 && n != -EAGAIN)
        printf("error reading events (%s)\n", strerror(-n));
    return 1;
}
static pthread_t accel_thread_tid;
static void *accel_thread(void *arg)
{
    sp<Looper> loop = new Looper(false);
    loop->addFd(accelq->getFd(), 0, ALOOPER_EVENT_INPUT, receiver, NULL);
    do {
        //printf("about to poll...\n");
        int32_t ret = loop->pollOnce(-1);
        //printf("ret %d\n", ret);
    } while (1);
    return NULL;
}

void start_accelerometer(void)
{
static int running = 0;
    if (running)
        return;
    SensorManager& mgr(SensorManager::getInstance());
    Sensor const* const* list;
    ssize_t count = mgr.getSensorList(&list);
    printf("numSensors=%d\n", int(count));
    accelq = mgr.createEventQueue();
    printf("queue=%p\n", accelq.get());
    oldTimeStamp = systemTime();
    Sensor const* accelerometer = mgr.getDefaultSensor(Sensor::TYPE_ACCELEROMETER);
    printf("accelerometer=%p (%s)\n", accelerometer, accelerometer->getName().string());
    accelq->enableSensor(accelerometer);
    accelq->setEventRate(accelerometer, ms2ns(10));
    int rc = pthread_create(&accel_thread_tid, NULL, accel_thread, NULL);
    if (rc != 0) {
        printf ("accel_thread creation failed\n");
        exit(-1);
    }
    running = 1;
}
