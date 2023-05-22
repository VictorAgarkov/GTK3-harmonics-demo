#ifndef GOODS_H_INCLUDED
#define GOODS_H_INCLUDED

// полезные макросы
#define BV(a) (1UL << (a))
#define NUMOFARRAY(a)    (sizeof(a) / sizeof(a[0]))

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
//#define abs(a) ((a) > 0 ? (a) : -(a))
#define lim(a,b,c) max((a), min((b),(c)))    // a <= b <= c

#ifndef NULL
#define NULL   0U
#endif // NULL

#define disable_irq asm volatile ("cpsid i")
#define enable_irq  asm volatile ("cpsie i")

#define CHARS2INT(a0, a1, a2, a3) (((a0) << 0) | ((a1) << 8) | ((a2) << 16) | ((a3) << 24))

#endif /* GOODS_H_INCLUDED */
