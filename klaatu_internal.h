
#include "klaatu_api.h"

extern KlaatuAPITemplate *client;
extern void enable_touch(uint32_t display_w, uint32_t display_h);
extern void event_process(void);
void start_accelerometer(void);
extern size_t event_indication;
extern int event_thread_stop;
