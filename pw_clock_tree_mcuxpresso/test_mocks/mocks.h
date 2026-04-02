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

#include <cstdint>

#include "fsl_clock.h"
#include "fsl_power.h"

namespace pw::clock_tree {

// Mock state for validation
struct MockSdkState {
  uint32_t fro_enabled_mask = 0;
  bool lp_osc_clk_enabled = false;
  uint32_t mclk_freq = 0xFFFFFFFF;
  uint32_t clkin_freq = 0xFFFFFFFF;
  clock_frg_clk_config_t frg_config;
  bool frg_config_set = false;
  clock_attach_id_t last_attached_clk = 0;
  clock_div_name_t last_div_name = 0xFFFFFFFF;
  uint32_t last_div_val = 0xFFFFFFFF;
  clock_audio_pll_config_t audio_pll_config;
  bool audio_pll_config_set = false;
  clock_sys_pll_config_t sys_pll_config;
  bool sys_pll_config_set = false;
  bool osc32k_enabled = false;
  clock_ip_name_t last_enabled_clock_ip = 0xFFFFFFFF;
  clock_ip_name_t last_disabled_clock_ip = 0xFFFFFFFF;
  bool lp_osc_pd_disabled = false;
  uint32_t attach_clk_call_count = 0;

  void Reset();
};

extern MockSdkState sdk_state;

}  // namespace pw::clock_tree
