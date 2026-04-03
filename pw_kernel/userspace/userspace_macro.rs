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
use proc_macro::TokenStream;
use proc_macro2::Span;
use quote::quote;
use syn::spanned::Spanned;
use syn::{Ident, ItemFn, ReturnType, Type, Visibility, parse};

fn validate_and_set_entry_ident(
    args: TokenStream,
    input: TokenStream,
) -> Result<ItemFn, TokenStream> {
    let mut f = match parse::<ItemFn>(input) {
        Ok(item_fn) => item_fn,
        Err(e) => return Err(e.to_compile_error().into()),
    };

    if !args.is_empty() {
        return Err(
            parse::Error::new(Span::call_site(), "`#[entry]` accepts no arguments")
                .to_compile_error()
                .into(),
        );
    }

    // check the function signature
    let valid_signature = f.sig.constness.is_none()
        && f.sig.asyncness.is_none()
        && f.vis == Visibility::Inherited
        && f.sig.abi.is_none()
        && (f.sig.inputs.is_empty() || f.sig.inputs.len() == 1)
        && f.sig.generics.params.is_empty()
        && f.sig.generics.where_clause.is_none()
        && f.sig.variadic.is_none()
        && match f.sig.output {
            ReturnType::Default => false,
            ReturnType::Type(_, ref ty) => {
                matches!(**ty, Type::Never(_))
            }
        };

    if !valid_signature {
        return Err(parse::Error::new(
            f.span(),
            "`#[entry]` function must have signature `fn() -> !` or `fn(index: usize) -> !` or `fn(index: usize) -> !`",
        )
        .to_compile_error()
        .into());
    }

    if f.sig.inputs.len() == 1 {
        let arg = &f.sig.inputs[0];
        let valid_arg = if let syn::FnArg::Typed(pat_type) = arg {
            if let syn::Type::Path(type_path) = &*pat_type.ty {
                type_path.path.is_ident("usize")
            } else {
                false
            }
        } else {
            false
        };

        if !valid_arg {
            return Err(parse::Error::new(
                arg.span(),
                "`#[entry]` argument must be of type `usize`",
            )
            .to_compile_error()
            .into());
        }
    }

    f.sig.ident = Ident::new(&format!("_start_{}", f.sig.ident), Span::call_site());
    Ok(f)
}

#[proc_macro_attribute]
pub fn arm_cortex_m_entry(args: TokenStream, input: TokenStream) -> TokenStream {
    let f = validate_and_set_entry_ident(args, input).unwrap();

    let asm = include_str!("arm_cortex_m/entry.s");
    quote!(
        use core::arch::global_asm;
        global_asm!(#asm, options(raw));

        #[unsafe(no_mangle)]
        #[unsafe(export_name = "main")]
        extern "C" #f
    )
    .into()
}

#[proc_macro_attribute]
pub fn riscv_entry(args: TokenStream, input: TokenStream) -> TokenStream {
    let f = validate_and_set_entry_ident(args, input).unwrap();

    let asm = include_str!("riscv/entry.s");
    quote!(
        use core::arch::global_asm;
        global_asm!(#asm, options(raw));

        #[unsafe(no_mangle)]
        #[unsafe(export_name = "main")]
        extern "C" #f
    )
    .into()
}

fn validate_and_set_process_entry_ident(
    args: TokenStream,
    input: TokenStream,
) -> Result<ItemFn, TokenStream> {
    let mut f = match parse::<ItemFn>(input) {
        Ok(item_fn) => item_fn,
        Err(e) => return Err(e.to_compile_error().into()),
    };

    if args.is_empty() {
        return Err(parse::Error::new(
            Span::call_site(),
            "`#[process_entry]` requires a process name argument",
        )
        .to_compile_error()
        .into());
    }

    let name_str = args.to_string().trim().replace('"', "");

    // check the function signature
    let valid_signature = f.sig.constness.is_none()
        && f.sig.asyncness.is_none()
        && f.vis == Visibility::Inherited
        && f.sig.abi.is_none()
        && f.sig.inputs.is_empty()
        && f.sig.generics.params.is_empty()
        && f.sig.generics.where_clause.is_none()
        && f.sig.variadic.is_none()
        && match f.sig.output {
            ReturnType::Default => false,
            ReturnType::Type(_, ref ty) => {
                matches!(**ty, Type::Never(_))
            }
        };

    if !valid_signature {
        return Err(parse::Error::new(
            f.span(),
            "`#[process_entry]` function must have signature `fn() -> !`",
        )
        .to_compile_error()
        .into());
    }

    // Rename to _entry_{name}
    f.sig.ident = Ident::new(&format!("_entry_{}", name_str), Span::call_site());
    Ok(f)
}

#[proc_macro_attribute]
pub fn process_entry(args: TokenStream, input: TokenStream) -> TokenStream {
    let f = match validate_and_set_process_entry_ident(args, input) {
        Ok(f) => f,
        Err(e) => return e,
    };

    quote!(
        #[unsafe(no_mangle)]
        extern "C" #f
    )
    .into()
}
