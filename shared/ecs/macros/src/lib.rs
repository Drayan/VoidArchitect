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
