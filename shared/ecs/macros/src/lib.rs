extern crate proc_macro;
use proc_macro::TokenStream;
use quote::quote;
use syn::{DeriveInput, parse_macro_input};

#[proc_macro_derive(Component)]
pub fn derive_component(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = input.ident;

    // Ensure the type implements Send, Sync and 'static.
    // The generated impl will have these bounds.
    let (impl_generics, ty_generics, where_clause) = input.generics.split_for_impl();

    // Add our bounds to the existing where clause, or create a new one.
    let mut extended_where_clause =
        where_clause.cloned().unwrap_or_else(|| syn::parse_quote!(where));
    extended_where_clause.predicates.push(syn::parse_quote!(Self: 'static + Send + Sync));

    let expanded = quote! {
        impl #impl_generics Component for #name #ty_generics #extended_where_clause {}
    };

    TokenStream::from(expanded)
}

#[proc_macro]
pub fn impl_component_bundle_for_tuples(_input: TokenStream) -> TokenStream {
    let mut generated_code = quote! {};

    for i in 1..=16 {
        // Implement for tuples of size 1 to 16
        let type_params: Vec<_> = (0..i)
            .map(|j| {
                let ident =
                    syn::Ident::new(&format!("T{}", j), proc_macro2::Span::call_site());
                quote! { #ident: Component }
            })
            .collect();

        let tuple_types: Vec<_> = (0..i)
            .map(|j| syn::Ident::new(&format!("T{}", j), proc_macro2::Span::call_site()))
            .collect();

        let tuple_fields: Vec<_> = (0..i).map(|j| syn::Index::from(j)).collect();

        let impl_code = quote! {
            impl<#(#type_params),*> ComponentBundle for (#(#tuple_types,)*) {
                fn get_component_type_ids(&self) -> std::collections::BTreeSet<ComponentTypeId> {
                    let mut set = std::collections::BTreeSet::new();
                    #(
                        set.insert(ComponentTypeId::of::<#tuple_types>());
                    )*
                    set
                }

                fn into_components_data(self) -> std::collections::HashMap<ComponentTypeId, Box<dyn std::any::Any>> {
                    let mut map = std::collections::HashMap::new();
                    #(
                        map.insert(ComponentTypeId::of::<#tuple_types>(), Box::new(self.#tuple_fields) as Box<dyn std::any::Any>);
                    )*
                    map
                }
            }
        };
        generated_code.extend(impl_code);
    }

    TokenStream::from(generated_code)
}
