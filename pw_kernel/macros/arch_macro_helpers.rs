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

use syn::spanned::Spanned;

pub fn validate_handler_abi(handler: &syn::ItemFn) -> syn::Result<()> {
    use syn::Error;
    let abi = handler.sig.abi.as_ref().ok_or_else(|| {
        Error::new(
            handler.sig.span(),
            "Handler missing an ABI.  Annotate with `extern \"C\"`",
        )
    })?;

    let abi_name = abi.name.as_ref().ok_or_else(|| {
        Error::new(
            handler.sig.span(),
            "Handler missing ABI name. Annotate with `extern \"C\"`",
        )
    })?;

    if abi_name.value() != "C" {
        return Err(Error::new(
            handler.sig.span(),
            "Handler ABI must be C. Annotate with `extern \"C\"`",
        ));
    }

    Ok(())
}

pub fn validate_interrupt_handler_args(handler: &syn::ItemFn) -> syn::Result<()> {
    use syn::{Error, FnArg, Type};
    if handler.sig.inputs.len() != 1 {
        return Err(Error::new(
            handler.sig.span(),
            "Interrupt handler must take exactly one argument, e.g., `from_userspace: bool`",
        ));
    }

    let FnArg::Typed(pat_ty) = &handler.sig.inputs.first().unwrap() else {
        return Err(Error::new(
            handler.sig.span(),
            "Interrupt handler must take an explicit typed argument, e.g., `from_userspace: bool`",
        ));
    };

    let Type::Path(type_path) = &*pat_ty.ty else {
        return Err(Error::new(
            handler.sig.span(),
            "Interrupt handler argument must be of type `bool`, e.g., `from_userspace: bool`",
        ));
    };

    if !type_path.path.is_ident("bool") {
        return Err(Error::new(
            handler.sig.span(),
            "Interrupt handler argument must be of type `bool`, e.g., `from_userspace: bool`",
        ));
    }

    Ok(())
}
