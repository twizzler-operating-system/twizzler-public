#pragma once

#ifdef __cplusplus
#define _Alignas(x) alignas(x)
#define _Bool bool
#else
#ifndef static_assert
#define static_assert _Static_assert
#endif
#endif
