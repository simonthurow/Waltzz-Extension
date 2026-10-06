#[macro_use]
extern crate afl;
extern crate wasmtime;

fn main() {
    fuzz!(|data: &[u8]| {
        let engine = wasmtime::Engine::default();
        let mut store = wasmtime::Store::new(&engine, ());
        let Ok(module) = wasmtime::Module::from_binary(&engine, data) else {return};
        let Ok(instance) = wasmtime::Instance::new(&mut store, &module, &[]) else {return};
        let Ok(main_func) = instance.get_typed_func::<(), ()>(&mut store, "main") else { return};
        let _ = main_func.call(&mut store, ());
    });
}
