/*
 * Compatibility symbols for toolchains that do not ship libatomic.
 * Some prebuilt managed components reference __atomic_fetch_add_1/sub_1.
 */

unsigned char __atomic_fetch_add_1(volatile void *ptr, unsigned char val, int memorder)
{
    (void)memorder;
    return __sync_fetch_and_add((volatile unsigned char *)ptr, val);
}

unsigned char __atomic_fetch_sub_1(volatile void *ptr, unsigned char val, int memorder)
{
    (void)memorder;
    return __sync_fetch_and_sub((volatile unsigned char *)ptr, val);
}
