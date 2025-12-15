// Copyright 2025 The Pigweed Authors
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

use std::fs;
use std::path::Path;

use anyhow::{Context, Result, anyhow};
use object::{Endian, Object, ObjectSection};

pub struct StackInfo {
    pub name: String,
    pub stack_addr: u64,
    pub stack_size: u64,
}

pub struct ImageInfo {
    pub stacks: Vec<StackInfo>,
}

impl ImageInfo {
    pub fn new(path: &Path) -> Result<Self> {
        let bin_data = fs::read(path).context("Failed to read ELF file")?;
        let obj_file = object::File::parse(&*bin_data).context("Failed to parse ELF file")?;

        let section = obj_file
            .section_by_name(".pw_kernel.annotations.stack")
            .context("Failed to find .pw_kernel.annotations.stack section")?;

        let data = section.data().context("Failed to read section data")?;

        let is_64 = obj_file.is_64();
        let entry_size = if is_64 { 32 } else { 16 };

        if data.len() % entry_size != 0 {
            return Err(anyhow!(
                "Stack annotation section size is not a multiple of {}",
                entry_size
            ));
        }

        let endian = obj_file.endianness();
        let mut stacks = Vec::new();

        for entry_data in data.chunks(entry_size) {
            let name_addr = Self::extract_usize_field(endian, entry_data, 0, is_64)?;
            let name_len = Self::extract_usize_field(endian, entry_data, 1, is_64)?;
            let stack_addr = Self::extract_usize_field(endian, entry_data, 2, is_64)?;
            let stack_size = Self::extract_usize_field(endian, entry_data, 3, is_64)?;

            let name = Self::read_string(&obj_file, name_addr, name_len)
                .context("Failed to read stack name")?;

            stacks.push(StackInfo {
                name,
                stack_addr,
                stack_size,
            });
        }

        Ok(ImageInfo { stacks })
    }

    fn extract_usize_field(
        endian: object::Endianness,
        chunk: &[u8],
        index: usize,
        is_64: bool,
    ) -> Result<u64> {
        if is_64 {
            let start = index * 8;
            let bytes = chunk
                .get(start..start + 8)
                .context("Chunk too small for 64-bit field")?;
            Ok(endian.read_u64_bytes(bytes.try_into()?))
        } else {
            let start = index * 4;
            let bytes = chunk
                .get(start..start + 4)
                .context("Chunk too small for 32-bit field")?;
            Ok(u64::from(endian.read_u32_bytes(bytes.try_into()?)))
        }
    }

    fn read_data<'a>(obj_file: &'a object::File, addr: u64, len: u64) -> Result<&'a [u8]> {
        // Find the section containing the address
        for section in obj_file.sections() {
            let section_addr = section.address();
            let section_size = section.size();
            if addr >= section_addr && addr + len <= section_addr + section_size {
                let data = section.data()?;
                let offset = usize::try_from(addr - section_addr).context("Offset too large")?;
                return Ok(&data[offset..offset + usize::try_from(len).context("Len too large")?]);
            }
        }

        Err(anyhow!("Could not find address {:#x} in any section", addr))
    }

    fn read_string(obj_file: &object::File, addr: u64, len: u64) -> Result<String> {
        let bytes = Self::read_data(obj_file, addr, len)?;
        Ok(String::from_utf8_lossy(bytes).into_owned())
    }
}
