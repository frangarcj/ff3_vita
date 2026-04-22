/*
 * softfp_wrappers.hpp — C++ template infrastructure for softfp/hardfp ABI bridging
 *
 * When compiling with -mfloat-abi=hard, our functions expect float/double args
 * in VFP registers (s0-s15, d0-d7). But the Android .so uses softfp ABI and
 * passes floats in integer registers (r0-r3).
 *
 * This header provides:
 * - SoftfpWrap<Func> template: wraps a function pointer with pcs("aapcs") attribute
 *   so the .so can call it using softfp convention
 * - WRAP(func) macro: returns wrapped or direct address depending on whether
 *   the function signature contains float/double types
 * - SOFTFP_CALL(type, addr) macro: casts a .so function address to a softfp
 *   callable pointer (for calling INTO the .so from hardfp code)
 *
 * The wrapper function receives args via softfp (r0-r3), and internally calls
 * the original through a softfp function pointer cast, ensuring the entire
 * call chain uses consistent softfp ABI at the boundary.
 */

#ifndef SOFTFP_WRAPPERS_HPP
#define SOFTFP_WRAPPERS_HPP

#include <cstdint>

#define SOFTFP_ABI __attribute__((pcs("aapcs")))

/* ============================================================
 * Type traits: detect float/double in function signatures
 * ============================================================ */

template<typename T> struct is_fp { static constexpr bool value = false; };
template<> struct is_fp<float>  { static constexpr bool value = true; };
template<> struct is_fp<double> { static constexpr bool value = true; };

template<typename... Ts>
constexpr bool any_fp_v = (is_fp<Ts>::value || ...);

/* ============================================================
 * SoftfpWrap<Func> — auto-generates a softfp-ABI wrapper
 *
 * Usage:
 *   SoftfpWrap<cosf>::get()     → address of softfp wrapper for cosf
 *   SoftfpWrap<atoi>::get()     → address of atoi directly (no floats)
 *   SoftfpWrap<cosf>::needs_wrap → true (has float in signature)
 *   SoftfpWrap<atoi>::needs_wrap → false
 * ============================================================ */

template<auto Func>
struct SoftfpWrap;

template<typename Ret, typename... Args, Ret(*Func)(Args...)>
struct SoftfpWrap<Func> {
    static constexpr bool needs_wrap = any_fp_v<Ret, Args...>;

    /*
     * The wrapper function:
     * - Declared SOFTFP_ABI → receives args from softfp callers via r0-r3
     * - Internally calls the original function
     *   (compiler handles the ABI transition since wrapper is compiled with
     *    the same flags as the rest of our code)
     */
    static SOFTFP_ABI Ret wrapped(Args... args) {
        return Func(args...);
    }

    /* Returns wrapper address if float types present, direct address otherwise */
    static constexpr uintptr_t get() {
        if constexpr (needs_wrap)
            return (uintptr_t)&wrapped;
        else
            return (uintptr_t)Func;
    }
};

/* ============================================================
 * Convenience macros
 * ============================================================ */

/* For the default_dynlib table: wraps function if it has float args/return */
#define WRAP(func) SoftfpWrap<func>::get()

/* For calling INTO the .so (reverse direction): cast a .so function pointer
 * to use softfp calling convention so our hardfp code passes args correctly.
 *
 * Usage:
 *   auto touch_fn = SOFTFP_CALL(int(*)(int,int,int,int,float,float,float,float,unsigned int), addr);
 *   touch_fn(0, 0, n, n, x1, y1, x2, y2, mask);
 */
#define SOFTFP_CALL(type, addr) ((SOFTFP_ABI type)(addr))

/* Declare a function pointer type with softfp ABI */
#define SOFTFP_FN(ret, name, ...) ret (SOFTFP_ABI *name)(__VA_ARGS__)

#endif /* SOFTFP_WRAPPERS_HPP */
