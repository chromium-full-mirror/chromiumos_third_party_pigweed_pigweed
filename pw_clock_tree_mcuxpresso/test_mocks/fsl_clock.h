/*
 * Copyright 2026 The Pigweed Authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t status_t;
#define kStatus_Success 0

typedef struct {
  volatile uint32_t FRODIVOEN;
  volatile uint32_t SYSOSCBYPASS;
  volatile uint32_t SYSPLL0CLKSEL;
  volatile uint32_t SYSPLL0CTL0;
  volatile uint32_t SYSPLL0PFD;
  volatile uint32_t OSC32KHZCTL0;
  volatile uint32_t A32KHZWAKECLKSEL;
  volatile uint32_t A32KHZWAKECLKDIV;
} CLKCTL0_Type;

typedef struct {
  volatile uint32_t AUDIOPLL0CLKSEL;
  volatile uint32_t AUDIOPLL0CTL0;
  volatile uint32_t AUDIOPLL0PFD;
} CLKCTL1_Type;

extern CLKCTL0_Type* CLKCTL0;
extern CLKCTL1_Type* CLKCTL1;

#define CLKCTL0_SYSOSCBYPASS_SEL_MASK (0x7U)
#define CLKCTL0_SYSOSCBYPASS_SEL_SHIFT (0U)
#define CLKCTL0_SYSOSCBYPASS_SEL(x)                                  \
  (((uint32_t)(((uint32_t)(x)) << CLKCTL0_SYSOSCBYPASS_SEL_SHIFT)) & \
   CLKCTL0_SYSOSCBYPASS_SEL_MASK)

#define CLKCTL1_AUDIOPLL0CTL0_BYPASS_MASK (0x1U)
#define CLKCTL0_SYSPLL0CTL0_BYPASS_MASK (0x1U)

#define CLKCTL0_FRODIVOEN_FRO_DIV1_O_EN_MASK (0x1U)
#define CLKCTL0_FRODIVOEN_FRO_DIV2_O_EN_MASK (0x2U)
#define CLKCTL0_FRODIVOEN_FRO_DIV4_O_EN_MASK (0x4U)
#define CLKCTL0_FRODIVOEN_FRO_DIV8_O_EN_MASK (0x8U)
#define CLKCTL0_FRODIVOEN_FRO_DIV16_O_EN_MASK (0x10U)

#define CLKCTL0_OSC32KHZCTL0_ENA32KHZ_MASK (0x1U)

typedef enum _clock_fro_output_en {
  kCLOCK_FroDiv1OutEn = CLKCTL0_FRODIVOEN_FRO_DIV1_O_EN_MASK,
  kCLOCK_FroDiv2OutEn = CLKCTL0_FRODIVOEN_FRO_DIV2_O_EN_MASK,
  kCLOCK_FroDiv4OutEn = CLKCTL0_FRODIVOEN_FRO_DIV4_O_EN_MASK,
  kCLOCK_FroDiv8OutEn = CLKCTL0_FRODIVOEN_FRO_DIV8_O_EN_MASK,
  kCLOCK_FroDiv16OutEn = CLKCTL0_FRODIVOEN_FRO_DIV16_O_EN_MASK,
} clock_fro_output_en_t;

typedef enum _clock_pfd {
  kCLOCK_Pfd0 = 0U,
  kCLOCK_Pfd1 = 1U,
  kCLOCK_Pfd2 = 2U,
  kCLOCK_Pfd3 = 3U,
} clock_pfd_t;

typedef uint32_t clock_attach_id_t;
typedef uint32_t clock_div_name_t;
typedef uint32_t clock_ip_name_t;

// Mock values for selectors
#define kFRG_to_FLEXCOMM0 0x100
#define kNONE_to_FLEXCOMM0 0x101
#define kFRO_DIV8_to_I3C_CLK 0x102
#define kNONE_to_I3C_CLK 0x103
#define kNONE_to_CTIMER0 0x104
// inclusive-language: disable
#define kMASTER_CLK_to_CTIMER0 0x105
#define kMASTER_CLK_to_CTIMER1 0x106
// inclusive-language: enable
#define kFRO_DIV1_to_CTIMER1 0x107
#define kNONE_to_CTIMER1 0x108

// Mock values for clock IPs
#define kCLOCK_Flexcomm0 0x200
#define kCLOCK_I3c0 0x201
#define kCLOCK_Ct32b0 0x202
#define kCLOCK_Ct32b1 0x203

// Mock values for dividers
#define kCLOCK_DivI3cClk 0x300

typedef enum _sys_pll_src {
  kCLOCK_SysPllFroDiv8Clk = 0,
  kCLOCK_SysPllXtalIn = 1,
  kCLOCK_SysPllNone = 7
} sys_pll_src_t;

typedef enum _sys_pll_mult {
  kCLOCK_SysPllMult16 = 0,
  kCLOCK_SysPllMult20 = 1,
} sys_pll_mult_t;

typedef struct _clock_sys_pll_config {
  sys_pll_src_t sys_pll_src;
  uint32_t numerator;
  uint32_t denominator;
  sys_pll_mult_t sys_pll_mult;
} clock_sys_pll_config_t;

typedef enum _audio_pll_src {
  kCLOCK_AudioPllFroDiv8Clk = 0,
  kCLOCK_AudioPllXtalIn = 1,
  kCLOCK_AudioPllNone = 7
} audio_pll_src_t;

typedef enum _audio_pll_mult {
  kCLOCK_AudioPllMult16 = 0,
} audio_pll_mult_t;

typedef struct _clock_audio_pll_config {
  audio_pll_src_t audio_pll_src;
  uint32_t numerator;
  uint32_t denominator;
  audio_pll_mult_t audio_pll_mult;
} clock_audio_pll_config_t;

typedef struct _clock_frg_clk_config {
  uint8_t num;
  enum {
    kCLOCK_FrgMainClk = 0,
    kCLOCK_FrgPllDiv = 1,
    kCLOCK_FrgFroDiv4 = 2,
    kCLOCK_FrgNone = 7,
  } sfg_clock_src;
  uint8_t divider;
  uint8_t mult;
} clock_frg_clk_config_t;

void CLOCK_EnableFroClk(uint32_t div_out_enable_value);
void CLOCK_EnableLpOscClk(void);
void CLOCK_SetMclkFreq(uint32_t frequency);
void CLOCK_SetClkinFreq(uint32_t frequency);
void CLOCK_SetFRGClock(const clock_frg_clk_config_t* configuration);
void CLOCK_AttachClk(clock_attach_id_t clock);
void CLOCK_SetClkDiv(clock_div_name_t name, uint32_t value);
void CLOCK_InitAudioPll(const clock_audio_pll_config_t* configuration);
status_t CLOCK_InitAudioPfd(clock_pfd_t clock_pfd, uint8_t value);
void CLOCK_DeinitAudioPfd(clock_pfd_t clock_pfd);
void CLOCK_DeinitAudioPll(void);
void CLOCK_InitSysPll(const clock_sys_pll_config_t* configuration);
status_t CLOCK_InitSysPfd(clock_pfd_t clock_pfd, uint8_t value);
void CLOCK_DeinitSysPfd(clock_pfd_t clock_pfd);
void CLOCK_DeinitSysPll(void);
void CLOCK_EnableOsc32K(bool enable_osc32k);
void CLOCK_EnableClock(clock_ip_name_t clock);
void CLOCK_DisableClock(clock_ip_name_t clock);

#ifdef __cplusplus
}
#endif
