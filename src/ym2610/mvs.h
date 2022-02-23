#ifndef _MVS_H_
#define _MVS_H_

/* compatibility layer */
#define s8  int8_t
#define s16 int16_t
#define s32 int32_t

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t

#define ALIGN_DATA
#ifndef INLINE
#define INLINE static __inline__
#endif
#define SOUND_SAMPLES 512

#define Limit(val, max, min)                    \
{                                               \
        if (val > max) val = max;               \
        else if (val < min) val = min;          \
}

#endif
