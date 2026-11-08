#include <errno.h>
#include <pthread.h>

int pthread_join(pthread_t thread, void **return_value)
{
    (void)thread;
    (void)return_value;
    return EINVAL;
}
