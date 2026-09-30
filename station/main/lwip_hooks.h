#ifndef _LWIP_HOOKS_H
#define _LWIP_HOOKS_H

#include "lwip/arch.h"
#include "lwip/ip_addr.h"

struct netif;

#ifdef __cplusplus
extern "C"
{
#endif

    // Forward declaration matching LwIP's expected signature
    struct netif *ts_ip4_route_src_hook(const ip4_addr_t *src, const ip4_addr_t *dest);

#ifdef __cplusplus
}
#endif

// Safely undefine ESP-IDF's default definition in lwipopts.h
#ifdef LWIP_HOOK_IP4_ROUTE_SRC
#undef LWIP_HOOK_IP4_ROUTE_SRC
#endif

// Redefine it to your custom function
#define LWIP_HOOK_IP4_ROUTE_SRC(src, dest) ts_ip4_route_src_hook(src, dest)

#endif // _LWIP_HOOKS_H