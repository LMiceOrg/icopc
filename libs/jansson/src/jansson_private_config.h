#ifndef JASSON_PRIVATE_CONFIG_H
#define JASSON_PRIVATE_CONFIG_H

#ifndef HAVE_STDINT_H

#define int8_t char
#define uint8_t unsigned char

#define int16_t short
#define uint16_t unsigned short

#define int32_t int
#define uint32_t unsigned int

#define int64_t __int64
#define uint64_t unsigned __int64

#endif

#endif /* JASSON_PRIVATE_CONFIG_H */
