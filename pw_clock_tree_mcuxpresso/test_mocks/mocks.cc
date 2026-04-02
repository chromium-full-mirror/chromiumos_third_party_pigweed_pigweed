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

#include "mocks.h"

#include "fsl_clock.h"
#include "fsl_power.h"

namespace pw::clock_tree {

MockSdkState sdk_state;

void MockSdkState::Reset() {
  fro_enabled_mask = 0;
  lp_osc_clk_enabled = false;
  mclk_freq = 0xFFFFFFFF;
  clkin_freq = 0xFFFFFFFF;
  frg_config = {};
  frg_config_set = false;
  last_attached_clk = 0;
  last_div_name = 0xFFFFFFFF;
  last_div_val = 0xFFFFFFFF;
  audio_pll_config = {};
  audio_pll_config_set = false;
  sys_pll_config = {};
  sys_pll_config_set = false;
  osc32k_enabled = false;
  last_enabled_clock_ip = 0xFFFFFFFF;
  last_disabled_clock_ip = 0xFFFFFFFF;
  lp_osc_pd_disabled = false;
  *CLKCTL0 = {};
  *CLKCTL1 = {};
}

}  // namespace pw::clock_tree

using namespace pw::clock_tree;

// Implement mock SDK globals
CLKCTL0_Type mock_clkctl0;
CLKCTL1_Type mock_clkctl1;

extern "C" {
CLKCTL0_Type* CLKCTL0 = &mock_clkctl0;
CLKCTL1_Type* CLKCTL1 = &mock_clkctl1;

void CLOCK_EnableFroClk(uint32_t div_out_enable_value) {
  CLKCTL0->FRODIVOEN = div_out_enable_value;
  sdk_state.fro_enabled_mask = div_out_enable_value;
}
void CLOCK_EnableLpOscClk(void) { sdk_state.lp_osc_clk_enabled = true; }

void CLOCK_SetMclkFreq(uint32_t frequency) { sdk_state.mclk_freq = frequency; }

void CLOCK_SetClkinFreq(uint32_t frequency) {
  sdk_state.clkin_freq = frequency;
}

void CLOCK_SetFRGClock(const clock_frg_clk_config_t* configuration) {
  sdk_state.frg_config = *configuration;
  sdk_state.frg_config_set = true;
}

void CLOCK_AttachClk(clock_attach_id_t clock) {
  sdk_state.last_attached_clk = clock;
}

void CLOCK_SetClkDiv(clock_div_name_t name, uint32_t value) {
  sdk_state.last_div_name = name;
  sdk_state.last_div_val = value;
}

void CLOCK_InitAudioPll(const clock_audio_pll_config_t* configuration) {
  sdk_state.audio_pll_config = *configuration;
  sdk_state.audio_pll_config_set = true;
}

status_t CLOCK_InitAudioPfd(clock_pfd_t, uint8_t) { return kStatus_Success; }

void CLOCK_DeinitAudioPfd(clock_pfd_t) {}

void CLOCK_DeinitAudioPll(void) { sdk_state.audio_pll_config_set = false; }

void CLOCK_InitSysPll(const clock_sys_pll_config_t* configuration) {
  sdk_state.sys_pll_config = *configuration;
  sdk_state.sys_pll_config_set = true;
}

status_t CLOCK_InitSysPfd(clock_pfd_t, uint8_t) { return kStatus_Success; }

void CLOCK_DeinitSysPfd(clock_pfd_t) {}

void CLOCK_DeinitSysPll(void) { sdk_state.sys_pll_config_set = false; }

void CLOCK_EnableOsc32K(bool enable_osc32k) {
  sdk_state.osc32k_enabled = enable_osc32k;
}

void CLOCK_EnableClock(clock_ip_name_t clock) {
  sdk_state.last_enabled_clock_ip = clock;
}

void CLOCK_DisableClock(clock_ip_name_t clock) {
  sdk_state.last_disabled_clock_ip = clock;
}

void POWER_DisablePD(pd_bit_t enable_bit) {
  if (enable_bit == kPDRUNCFG_PD_LPOSC)
    sdk_state.lp_osc_pd_disabled = true;
}

void POWER_EnablePD(pd_bit_t enable_bit) {
  if (enable_bit == kPDRUNCFG_PD_LPOSC)
    sdk_state.lp_osc_pd_disabled = false;
}
}
