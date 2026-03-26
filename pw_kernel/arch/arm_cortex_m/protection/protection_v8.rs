// Copyright 2026 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

//! PMSAv8 (ARMv8-M) MPU region encoding.
//!
//! PMSAv8 uses a base/limit address model with MAIR-indexed memory attributes,
//! supporting arbitrary region sizes without the power-of-2 alignment
//! constraints of PMSAv7.

use memory_config::{MemoryRegion, MemoryRegionType};

use super::LOG_MPU;
use crate::regs::Regs;
use crate::regs::mpu::*;

#[repr(u8)]
enum AttrIndex {
    NormalMemoryRO = 0,
    NormalMemoryRW = 1,
    DeviceMemory = 2,
}

/// PMSAv8 MPU region descriptor (RBAR + RLAR register pair).
#[derive(Copy, Clone)]
pub struct MpuRegion {
    rbar: RbarVal,
    rlar: RlarVal,
}

impl MpuRegion {
    pub(super) const fn const_default() -> Self {
        Self {
            rbar: RbarVal::const_default(),
            rlar: RlarVal::const_default(),
        }
    }

    pub(super) const fn from_memory_region(region: &MemoryRegion) -> Self {
        // TODO: handle unaligned regions.  Fail/Panic?
        let (xn, sh, ap, attr_index) = match region.ty {
            MemoryRegionType::ReadOnlyData => (
                /* xn */ true,
                RbarSh::OuterShareable,
                RbarAp::RoAny,
                AttrIndex::NormalMemoryRO,
            ),
            MemoryRegionType::ReadWriteData => {
                (
                    /* xn */ true,
                    RbarSh::NonShareable,
                    RbarAp::RwAny,
                    AttrIndex::NormalMemoryRW,
                )
            }
            MemoryRegionType::ReadOnlyExecutable => {
                (
                    /* xn */ false,
                    RbarSh::OuterShareable,
                    RbarAp::RoAny,
                    AttrIndex::NormalMemoryRO,
                )
            }
            MemoryRegionType::ReadWriteExecutable => {
                (
                    /* xn */ false,
                    RbarSh::OuterShareable,
                    RbarAp::RwAny,
                    AttrIndex::NormalMemoryRW,
                )
            }
            MemoryRegionType::Device => {
                (
                    /* xn */ true,
                    RbarSh::OuterShareable,
                    RbarAp::RoAny,
                    AttrIndex::DeviceMemory,
                )
            }
        };

        let start = region.start;
        // MemoryRegion's end is exclusive and the MPU forces the lower 5 bits
        // to be 0x1f.
        let end = region.end - 1;

        // pw_cast::CastInto can't be used in const context; usizes are explicitly
        // cast to u32s.
        #[expect(clippy::cast_possible_truncation)]
        Self {
            rbar: RbarVal::const_default()
                .with_xn(xn)
                .with_sh(sh)
                .with_ap(ap)
                .with_base(start as u32),

            rlar: RlarVal::const_default()
                .with_en(true)
                .with_attrindx(attr_index as u8)
                .with_pxn(false)
                .with_limit(end as u32),
        }
    }

    pub(super) fn write_to_mpu(&self, mpu: &mut Mpu, index: usize) {
        log_if::debug_if!(
            LOG_MPU,
            "MPU[{}]: RBAR=0x{:08X} RLAR=0x{:08X}",
            index as usize,
            self.rbar.0 as usize,
            self.rlar.0 as usize
        );

        pw_assert::debug_assert!(index < 255);
        #[expect(clippy::cast_possible_truncation)]
        {
            mpu.rnr.write(RnrVal::default().with_region(index as u8));
        }
        mpu.rbar.write(self.rbar);
        mpu.rlar.write(self.rlar);
    }

    pub(super) fn dump(&self, index: usize) {
        log_if::debug_if!(
            LOG_MPU,
            "MPU region {}: RBAR={:#010x}, RLAR={:#010x}",
            index as usize,
            self.rbar.0 as usize,
            self.rlar.0 as usize
        );
    }
}

/// Initialize MAIR registers with memory attribute encodings for PMSAv8.
pub(super) fn init_mair() {
    let mut mpu = Regs::get().mpu;

    // Attributes below are the recommended values from
    // https://developer.arm.com/documentation/107565/0101/Memory-protection/Memory-Protection-Unit
    let val = Mair0Val::default()
        // AttrIndex::NormalMemoryRO
        .with_attr0(MairAttr::normal_memory(
            MairNormalMemoryCaching::WriteBackNonTransientRO,
            MairNormalMemoryCaching::WriteBackNonTransientRO,
        ))
        // AttrIndex::NormalMemoryRW
        .with_attr1(MairAttr::normal_memory(
            MairNormalMemoryCaching::WriteBackNonTransientRW,
            MairNormalMemoryCaching::WriteBackNonTransientRW,
        ))
        // AttrIndex::DeviceMemory
        .with_attr2(MairAttr::device_memory(MairDeviceMemoryOrdering::nGnRE));
    mpu.mair0.write(val);
}
