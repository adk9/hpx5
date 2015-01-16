#ifndef PHOTON_COUNTER_H_
#define PHOTON_COUNTER_H_

#include <pthread.h>

#define DEFINE_COUNTER(name, type)              \
 static type name = 0;                          \
 static pthread_mutex_t name##_mtx;             \
                                                \
 static inline type get_##name() {              \
     type tmp;                                  \
     pthread_mutex_lock(&name##_mtx);           \
         tmp = name;                            \
     pthread_mutex_unlock(&name##_mtx);         \
     return tmp;                                \
 }                                              \
                                                \
 static inline type get_inc_##name() {          \
     type tmp;                                  \
     pthread_mutex_lock(&name##_mtx);           \
         tmp = name;                            \
         ++name;                                \
     pthread_mutex_unlock(&name##_mtx);         \
     return tmp;                                \
 }                                              \
                                                \
 static inline type get_dec_##name() {          \
     type tmp;                                  \
     pthread_mutex_lock(&name##_mtx);           \
         tmp = name;                            \
         --name;                                \
     pthread_mutex_unlock(&name##_mtx);         \
     return tmp;                                \
 }                                              \
                                                \
 static inline type get_set_##name(type val) {  \
     type tmp;                                  \
     pthread_mutex_lock(&name##_mtx);           \
         tmp = name;                            \
         name = val;                            \
     pthread_mutex_unlock(&name##_mtx);         \
     return tmp;                                \
 }

#define INIT_COUNTER(name, val) do {            \
    pthread_mutex_init(&name##_mtx, NULL);      \
    name = val;                                 \
} while(/*CONSTCOND*/0)

#define DESTROY_COUNTER(name) do {              \
    name = 0;                                   \
    pthread_mutex_destroy(&name##_mtx);         \
} while(/*CONSTCOND*/0)

#define GET_COUNTER(name) (get_##name())
#define INC_COUNTER(name) (get_inc_##name())
#define DEC_COUNTER(name) (get_dec_##name())

#endif /* PHOTON_COUNTER_H_ */
