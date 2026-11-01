/*
 * lxcompat.h : le strict nécessaire de l'API du noyau Linux pour compiler les fichiers
 * « matériel » d'ath5k hors de Linux (essais en espace utilisateur, puis OS/2).
 * Les services réels (accès aux registres, délais, mémoire DMA, journal) sont fournis
 * par la plate-forme, via osdep.h.
 */
#ifndef LXCOMPAT_H
#define LXCOMPAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "osdep.h"

/* La bibliothèque C définit toujours __BIG_ENDIAN (constante 4321) ; le noyau Linux ne
 * le définit que sur les machines gros-boutistes, et ath5k s'en sert pour faire inverser
 * les descripteurs par la puce. Le x86 est petit-boutiste. */
#undef __BIG_ENDIAN
#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN 1234
#endif

/* --- Types ------------------------------------------------------------------ */
typedef uint8_t u8;   typedef int8_t s8;
typedef uint16_t u16; typedef int16_t s16;
typedef uint32_t u32; typedef int32_t s32;
typedef uint64_t u64; typedef int64_t s64;
typedef u16 __le16; typedef u32 __le32; typedef u64 __le64;
typedef u16 __be16; typedef u32 __be32;
typedef u32 dma_addr_t;
typedef unsigned int gfp_t;
typedef unsigned int u_int;

/* --- Compilateur ------------------------------------------------------------ */
#define __iomem
#define __packed __attribute__((packed))
#define __aligned(x) __attribute__((aligned(x)))
#define __maybe_unused __attribute__((unused))
#define __always_unused __attribute__((unused))
#define __force
#define __must_check
#define __init
#define __exit
#define __read_mostly
#define __cold
#define __printf(a, b) __attribute__((format(printf, a, b)))
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define fallthrough __attribute__((fallthrough))
#define READ_ONCE(x) (*(volatile __typeof__(x) *)&(x))
#define WRITE_ONCE(x, v) (*(volatile __typeof__(x) *)&(x) = (v))
#define BUILD_BUG_ON(c) ((void)sizeof(char[1 - 2 * !!(c)]))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define BIT(n) (1UL << (n))
#define BIT_ULL(n) (1ULL << (n))
#define GENMASK(h, l) (((~0UL) << (l)) & (~0UL >> (31 - (h))))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define min_t(t, a, b) ((t)(a) < (t)(b) ? (t)(a) : (t)(b))
#define max_t(t, a, b) ((t)(a) > (t)(b) ? (t)(a) : (t)(b))
#define clamp(v, lo, hi) min(max(v, lo), hi)
#define clamp_t(t, v, lo, hi) min_t(t, max_t(t, v, lo), hi)
#define clamp_val(v, lo, hi) clamp_t(__typeof__(v), v, lo, hi)
#define abs(x) ((x) < 0 ? -(x) : (x))
#define swap(a, b) do { __typeof__(a) _t = (a); (a) = (b); (b) = _t; } while (0)
#define DIV_ROUND_UP(n, d) (((n) + (d) - 1) / (d))
#define DIV_ROUND_CLOSEST(x, d) (((x) + ((d) / 2)) / (d))
#define roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define offsetofend(T, m) (offsetof(T, m) + sizeof(((T *)0)->m))
#define container_of(p, T, m) ((T *)((char *)(p) - offsetof(T, m)))
#define struct_group(name, ...) union { struct { __VA_ARGS__ }; struct { __VA_ARGS__ } name; }
#define EXPORT_SYMBOL(x)
#define EXPORT_SYMBOL_GPL(x)
#define MODULE_LICENSE(x)
#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)
#define module_param_named(...)
#define MODULE_PARM_DESC(...)
#define KBUILD_MODNAME "athos"

/* --- Ordre des octets (x86 : petit-boutiste) -------------------------------- */
#define cpu_to_le16(x) ((u16)(x))
#define cpu_to_le32(x) ((u32)(x))
#define le16_to_cpu(x) ((u16)(x))
#define le32_to_cpu(x) ((u32)(x))
#define le16_to_cpus(p) ((void)0)
#define cpu_to_be16(x) ((u16)__builtin_bswap16(x))
#define be16_to_cpu(x) ((u16)__builtin_bswap16(x))
#define cpu_to_be32(x) ((u32)__builtin_bswap32(x))
#define be32_to_cpu(x) ((u32)__builtin_bswap32(x))
static inline u16 get_unaligned_le16(const void *p) { u16 v; memcpy(&v, p, 2); return v; }
static inline u32 get_unaligned_le32(const void *p) { u32 v; memcpy(&v, p, 4); return v; }
static inline void put_unaligned_le16(u16 v, void *p) { memcpy(p, &v, 2); }
static inline void put_unaligned_le32(u32 v, void *p) { memcpy(p, &v, 4); }

/* --- Bits ------------------------------------------------------------------- */
#define BITS_PER_LONG (8 * sizeof(long))
#define BITS_TO_LONGS(n) (((n) + BITS_PER_LONG - 1) / BITS_PER_LONG)
#define DECLARE_BITMAP(name, bits) unsigned long name[BITS_TO_LONGS(bits)]
static inline void set_bit(int n, unsigned long *a) { a[n / BITS_PER_LONG] |= 1UL << (n % BITS_PER_LONG); }
static inline void __set_bit(int n, unsigned long *a) { set_bit(n, a); }
static inline void clear_bit(int n, unsigned long *a) { a[n / BITS_PER_LONG] &= ~(1UL << (n % BITS_PER_LONG)); }
static inline void __clear_bit(int n, unsigned long *a) { clear_bit(n, a); }
static inline int test_bit(int n, const unsigned long *a) { return (a[n / BITS_PER_LONG] >> (n % BITS_PER_LONG)) & 1; }
static inline int test_and_set_bit(int n, unsigned long *a) { int o = test_bit(n, a); set_bit(n, a); return o; }
static inline int test_and_clear_bit(int n, unsigned long *a) { int o = test_bit(n, a); clear_bit(n, a); return o; }
static inline int hweight32(u32 v) { return __builtin_popcount(v); }
static inline int hweight8(u8 v) { return __builtin_popcount(v); }
static inline int ilog2(u32 v) { return 31 - __builtin_clz(v); }
static inline int fls(u32 v) { return v ? 32 - __builtin_clz(v) : 0; }
static inline unsigned long __ffs(unsigned long v) { return __builtin_ctzl(v); }
static inline bool is_power_of_2(unsigned long n) { return n && !(n & (n - 1)); }
static inline u32 rol32(u32 w, unsigned s) { return (w << s) | (w >> (32 - s)); }
static inline u32 bitrev32(u32 x) { u32 r = 0; for (int i = 0; i < 32; i++) { r = (r << 1) | (x & 1); x >>= 1; } return r; }

/* --- Mémoire ---------------------------------------------------------------- */
#define GFP_KERNEL 0
#define GFP_ATOMIC 0
static inline void *kzalloc(size_t n, gfp_t f) { (void)f; return calloc(1, n); }
static inline void *kmalloc(size_t n, gfp_t f) { (void)f; return malloc(n); }
static inline void *kcalloc(size_t c, size_t n, gfp_t f) { (void)f; return calloc(c, n); }
static inline void kfree(const void *p) { free((void *)p); }
#define kmemdup(p, n, f) memcpy(kmalloc(n, f), p, n)
#define kmalloc_array(c, n, f) kcalloc(c, n, f)
#define kzalloc_objs(T, n) ((T *)kcalloc((n), sizeof(T), 0))

/* --- Journal ---------------------------------------------------------------- */
#define KERN_ERR "erreur : "
#define KERN_WARNING "attention : "
#define KERN_INFO ""
#define KERN_DEBUG ""
#define KERN_NOTICE ""
#define printk(...) os_log(__VA_ARGS__)
#define pr_err(...) os_log("erreur : " __VA_ARGS__)
#define pr_warn(...) os_log("attention : " __VA_ARGS__)
#define pr_info(...) os_log(__VA_ARGS__)
#define pr_debug(...) ((void)0)
#define pr_notice(...) os_log(__VA_ARGS__)
#define pr_cont(...) os_log(__VA_ARGS__)
#define WARN_ON(c) ({ int _c = !!(c); if (_c) os_log("WARN_ON %s:%d\n", __FILE__, __LINE__); _c; })
#define WARN_ON_ONCE(c) WARN_ON(c)
#define WARN(c, ...) ({ int _c = !!(c); if (_c) os_log(__VA_ARGS__); _c; })
#define BUG_ON(c) do { if (c) os_fatal(__FILE__, __LINE__); } while (0)
#define BUG() os_fatal(__FILE__, __LINE__)

/* --- Délais ----------------------------------------------------------------- */
#define udelay(us) os_udelay(us)
#define ndelay(ns) os_udelay(((ns) + 999) / 1000)
#define mdelay(ms) os_udelay((ms) * 1000)
#define msleep(ms) os_udelay((ms) * 1000)
#define usleep_range(a, b) os_udelay(a)
#define HZ 1000
#define jiffies os_jiffies()
#define msecs_to_jiffies(m) (m)
#define jiffies_to_msecs(j) (j)
#define time_after(a, b) ((long)((b) - (a)) < 0)
#define time_before(a, b) time_after(b, a)
#define time_is_before_jiffies(a) time_after(jiffies, a)

/* --- Verrous et tâches : un seul fil d'exécution dans les essais ------------ */
typedef struct { int x; } spinlock_t;
struct mutex { int x; };
struct work_struct { int x; };
struct delayed_work { struct work_struct work; };
struct tasklet_struct { int x; };
struct timer_list { int x; };
struct list_head { struct list_head *next, *prev; };
struct completion { int x; };
typedef struct { int counter; } atomic_t;
#define spin_lock(l) ((void)(l))
#define spin_unlock(l) ((void)(l))
#define spin_lock_bh(l) ((void)(l))
#define spin_unlock_bh(l) ((void)(l))
#define spin_lock_irqsave(l, f) ((void)(l), (f) = 0)
#define spin_unlock_irqrestore(l, f) ((void)(l), (void)(f))
#define spin_lock_init(l) ((void)(l))
#define mutex_lock(m) ((void)(m))
#define mutex_unlock(m) ((void)(m))
#define mutex_init(m) ((void)(m))
#define lockdep_assert_held(m) ((void)(m))
#define DECLARE_EWMA(name, a, b) DEFINE_EWMA(name, a, b)
#define DEFINE_EWMA(name, a, b) struct ewma_##name { unsigned long internal; }; \
	static inline void ewma_##name##_init(struct ewma_##name *e) { e->internal = 0; } \
	static inline void ewma_##name##_add(struct ewma_##name *e, unsigned long v) { e->internal = e->internal ? (e->internal * 7 + v * 1024) / 8 : v * 1024; } \
	static inline unsigned long ewma_##name##_read(struct ewma_##name *e) { return e->internal / 1024; }

/* --- Divers périphériques --------------------------------------------------- */
struct led_classdev { int x; };
#ifdef ATHOS_TRACE
u32 os_mmio_read(volatile void *addr);
void os_mmio_write(u32 val, volatile void *addr);
#define ioread32(addr) os_mmio_read(addr)
#define iowrite32(val, addr) os_mmio_write((val), (addr))
#else
#define ioread32(addr) (*(volatile u32 *)(addr))
#define iowrite32(val, addr) (*(volatile u32 *)(addr) = (val))
#endif

/* --- PCI -------------------------------------------------------------------- */
struct device { int x; };
struct pci_dev { struct device dev; u16 vendor, device, subsystem_vendor, subsystem_device; u8 revision; };
struct platform_device { struct device dev; int id; };
#define to_platform_device(d) ((struct platform_device *)(d))
#define dev_name(d) "ath5k"
#define pci_is_pcie(p) 1
#define dev_err(d, ...) os_log("erreur : " __VA_ARGS__)
#define dev_warn(d, ...) os_log("attention : " __VA_ARGS__)
#define dev_info(d, ...) os_log(__VA_ARGS__)
#define dev_dbg(d, ...) ((void)0)

/* --- 802.11 (mac80211 / cfg80211), réduit à ce que touche le matériel -------- */
#define ETH_ALEN 6
#define FCS_LEN 4
enum nl80211_band { NL80211_BAND_2GHZ, NL80211_BAND_5GHZ, NL80211_BAND_60GHZ, NL80211_BAND_6GHZ, NL80211_BAND_S1GHZ, NL80211_BAND_LC, NUM_NL80211_BANDS };
enum nl80211_dfs_regions { NL80211_DFS_UNSET, NL80211_DFS_FCC, NL80211_DFS_ETSI, NL80211_DFS_JP };
enum nl80211_iftype { NL80211_IFTYPE_UNSPECIFIED, NL80211_IFTYPE_ADHOC, NL80211_IFTYPE_STATION, NL80211_IFTYPE_AP, NL80211_IFTYPE_AP_VLAN, NL80211_IFTYPE_WDS, NL80211_IFTYPE_MONITOR, NL80211_IFTYPE_MESH_POINT };
enum nl80211_reg_initiator { NL80211_REGDOM_SET_BY_CORE, NL80211_REGDOM_SET_BY_USER, NL80211_REGDOM_SET_BY_DRIVER, NL80211_REGDOM_SET_BY_COUNTRY_IE };
#define IEEE80211_CHAN_DISABLED   (1 << 0)
#define IEEE80211_CHAN_NO_IR      (1 << 1)
#define IEEE80211_CHAN_RADAR      (1 << 3)
struct ieee80211_channel {
	enum nl80211_band band;
	u32 center_freq;
	u16 hw_value;
	u32 flags;
	int max_antenna_gain, max_power, max_reg_power, orig_mag, orig_mpwr;
	bool beacon_found;
};
#define IEEE80211_RATE_SHORT_PREAMBLE (1 << 0)
struct ieee80211_rate { u32 flags; u16 bitrate; u16 hw_value, hw_value_short; };
struct ieee80211_supported_band { struct ieee80211_channel *channels; struct ieee80211_rate *bitrates; enum nl80211_band band; int n_channels, n_bitrates; };
struct ieee80211_hw { void *priv; };
struct ieee80211_vif { enum nl80211_iftype type; u8 addr[ETH_ALEN]; };
struct ieee80211_ops { int x; };
struct sk_buff { u8 *data; unsigned int len; };
struct wiphy { int x; };
struct regulatory_request { int x; };
struct ieee80211_regdomain { int x; };
struct cfg80211_chan_def { struct ieee80211_channel *chan; };
static inline int ieee80211_channel_to_frequency(int chan, enum nl80211_band band)
{
	if (band == NL80211_BAND_2GHZ) return chan == 14 ? 2484 : 2407 + chan * 5;
	return 5000 + chan * 5;
}
static inline int ieee80211_frequency_to_channel(int freq)
{
	if (freq == 2484) return 14;
	if (freq < 2484) return (freq - 2407) / 5;
	return (freq - 5000) / 5;
}
static inline bool is_valid_ether_addr(const u8 *a) { return !(a[0] & 1) && (a[0] | a[1] | a[2] | a[3] | a[4] | a[5]); }
static inline void ether_addr_copy(u8 *d, const u8 *s) { memcpy(d, s, ETH_ALEN); }
static inline void eth_zero_addr(u8 *a) { memset(a, 0, ETH_ALEN); }
#define ieee80211_hw_check(hw, f) 0

#define IEEE80211_MAX_FRAME_LEN 2352
#define IEEE80211_RATE_SUPPORTS_5MHZ  (1 << 5)
#define IEEE80211_RATE_SUPPORTS_10MHZ (1 << 6)
struct survey_info { struct ieee80211_channel *channel; u64 time, time_busy, time_rx, time_tx; s8 noise; u32 filled; };
static inline void eth_broadcast_addr(u8 *a) { memset(a, 0xff, ETH_ALEN); }
/* Durée d'une trame (802.11b ou g), comme ieee80211_generic_frame_duration. */
static inline __le16 ieee80211_generic_frame_duration(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
	enum nl80211_band band, size_t len, struct ieee80211_rate *rate)
{
	int r = rate->bitrate, dur;
	(void)hw; (void)vif;
	if (band == NL80211_BAND_5GHZ || (r != 10 && r != 20 && r != 55 && r != 110)) {
		dur = 16 + 16 + 4 + 4 * DIV_ROUND_UP((16 + 8 * ((int)len + 4) + 6) * 10, 4 * r);
	} else {
		dur = 10 + 144 + 48 + DIV_ROUND_UP(8 * ((int)len + 4) * 10, r);
	}
	return cpu_to_le16(dur);
}

/* --- Divers ----------------------------------------------------------------- */
#define sort(base, num, size, cmp, swp) qsort(base, num, size, cmp)
#define IS_ENABLED(x) 0
#define ENOTSUPP 524
static inline int net_ratelimit(void) { return 1; }
static inline s32 sign_extend32(u32 v, int idx) { u8 sh = 31 - idx; return (s32)(v << sh) >> sh; }
static inline unsigned long roundup_pow_of_two(unsigned long n) { unsigned long r = 1; while (r < n) r <<= 1; return r; }
#define local_irq_save(f) ((f) = 0)
#define local_irq_restore(f) ((void)(f))

#endif
