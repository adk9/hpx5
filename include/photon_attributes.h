#ifndef PHOTON_ATTRIBUTES_H
#define PHOTON_ATTRIBUTES_H

#define PHOTON_ALIGNED(N)       __attribute__((aligned(N)))
#define PHOTON_INTERNAL         __attribute__((visibility("internal")))
#define PHOTON_RETURNS_TWICE    __attribute__((returns_twice))
#define PHOTON_NORETURN         __attribute__((noreturn))
#define PHOTON_NOINLINE         __attribute__((noinline))
#define PHOTON_AWAYS_INLINE     __attribute__((always_inline))
#define PHOTON_OPTIMIZE(S)      __attribute__((optimize(S)))
#define PHOTON_MALLOC           __attribute__((malloc))
#define PHOTON_USED             __attribute__((used))
#define PHOTON_UNUSED           __attribute__((unused))
#define PHOTON_ASM(S)           __asm__(#S)
#define PHOTON_PACKED           __attribute__((packed))
#define PHOTON_NON_NULL(...)    __attribute__((nonnull(__VA_ARGS__)))
#define PHOTON_WEAK             __attribute__((weak))
#define PHOTON_CONSTRUCTOR      __attribute__((constructor))
#define PHOTON_DESTRUCTOR       __attribute__((destructor))

#ifdef __clang
#define PHOTON_NO_OPTIMIZE      PHOTON_OPTIMIZE((optnone))
#else
#define PHOTON_NO_OPTIMIZE      PHOTON_OPTIMIZE("O0")
#endif

#endif
