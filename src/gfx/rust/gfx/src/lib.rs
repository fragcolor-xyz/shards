#[cfg(feature = "wgpu-native")]
extern crate wgpu_native; 
 
#[cfg(feature = "tracy")]
mod tracy_impl {
    use tracy_client;
    #[no_mangle]
    pub unsafe extern "C" fn gfxTracyInit() {
        tracy_client::Client::start();
    }
}
