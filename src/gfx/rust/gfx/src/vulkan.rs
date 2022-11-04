use ash::vk;
use log;
use std::ffi::{c_char, c_void, CStr, CString};
use std::ptr::{null, null_mut};
use std::sync::Arc;
use wgpu_core as wgc;
use wgpu_hal::vulkan;
use wgpu_hal::{InstanceError, InstanceFlags};
use wgpu_native::{make_context_handle, Context};
use wgpu_types as wgt;

use crate::native;

#[repr(C)]
pub struct WGPUVKInstance {
    pub wgpu_instance: wgpu_native::native::WGPUInstance,
    pub vulkan_instance: vk::Instance,
}

// Modified version of wgpu-hal/src/vulkan/instance.rs
unsafe fn createInstanceRaw(
    desc: &native::WGPUInstanceDescriptorVK,
) -> Result<vulkan::Instance, InstanceError> {
    let name = "SomeName";
    let app_name = CString::new(name).unwrap();
    let engine_name = CStr::from_bytes_with_nul(b"wgpu-hal\0").unwrap();

    let flags = InstanceFlags::DEBUG | InstanceFlags::VALIDATION;

    let entry = match ash::Entry::load() {
        Ok(entry) => entry,
        Err(err) => {
            log::info!("Missing Vulkan entry points: {:?}", err);
            return Err(InstanceError);
        }
    };

    let driver_api_version = match entry.try_enumerate_instance_version() {
        // Vulkan 1.1+
        Ok(Some(version)) => version,
        Ok(None) => vk::API_VERSION_1_0,
        Err(err) => {
            log::warn!("try_enumerate_instance_version: {:?}", err);
            return Err(InstanceError);
        }
    };

    let app_info = vk::ApplicationInfo::builder()
        .application_name(app_name.as_c_str())
        .application_version(1)
        .engine_name(engine_name)
        .engine_version(2)
        .api_version(
            // Vulkan 1.0 doesn't like anything but 1.0 passed in here...
            if driver_api_version < vk::API_VERSION_1_1 {
                vk::API_VERSION_1_0
            } else {
                // This is the max Vulkan API version supported by `wgpu-hal`.
                //
                // If we want to increment this, there are some things that must be done first:
                //  - Audit the behavioral differences between the previous and new API versions.
                //  - Audit all extensions used by this backend:
                //    - If any were promoted in the new API version and the behavior has changed, we must handle the new behavior in addition to the old behavior.
                //    - If any were obsoleted in the new API version, we must implement a fallback for the new API version
                //    - If any are non-KHR-vendored, we must ensure the new behavior is still correct (since backwards-compatibility is not guaranteed).
                vk::HEADER_VERSION_COMPLETE
            },
        );

    let mut extensions = vulkan::Instance::required_extensions(&entry, flags)?;

    // Add extensions from descriptor
    let extra_extensions = std::slice::from_raw_parts(
        desc.requiredExtensions,
        desc.requiredExtensionCount as usize,
    )
    .into_iter()
    .map(|v| CStr::from_ptr(*v));
    for ext in extra_extensions {
        extensions.push(ext);
    }

    let instance_layers = entry.enumerate_instance_layer_properties().map_err(|e| {
        log::info!("enumerate_instance_layer_properties: {:?}", e);
        InstanceError
    })?;

    let nv_optimus_layer = CStr::from_bytes_with_nul(b"VK_LAYER_NV_optimus\0").unwrap();
    let has_nv_optimus = instance_layers
        .iter()
        .any(|inst_layer| CStr::from_ptr(inst_layer.layer_name.as_ptr()) == nv_optimus_layer);

    // Check requested layers against the available layers
    let layers = {
        let mut layers: Vec<&'static CStr> = Vec::new();
        if flags.contains(InstanceFlags::VALIDATION) {
            layers.push(CStr::from_bytes_with_nul(b"VK_LAYER_KHRONOS_validation\0").unwrap());
        }

        // Only keep available layers.
        layers.retain(|&layer| {
            if instance_layers
                .iter()
                .any(|inst_layer| CStr::from_ptr(inst_layer.layer_name.as_ptr()) == layer)
            {
                true
            } else {
                log::warn!("Unable to find layer: {}", layer.to_string_lossy());
                false
            }
        });
        layers
    };

    #[cfg(target_os = "android")]
    let android_sdk_version = {
        let properties = android_system_properties::AndroidSystemProperties::new();
        // See: https://developer.android.com/reference/android/os/Build.VERSION_CODES
        if let Some(val) = properties.get("ro.build.version.sdk") {
            match val.parse::<u32>() {
                Ok(sdk_ver) => sdk_ver,
                Err(err) => {
                    log::error!(
                        "Couldn't parse Android's ro.build.version.sdk system property ({val}): {err}"
                    );
                    0
                }
            }
        } else {
            log::error!("Couldn't read Android's ro.build.version.sdk system property");
            0
        }
    };
    #[cfg(not(target_os = "android"))]
    let android_sdk_version = 0;

    let vk_instance = {
        let str_pointers = layers
            .iter()
            .chain(extensions.iter())
            .map(|&s| {
                // Safe because `layers` and `extensions` entries have static lifetime.
                s.as_ptr()
            })
            .collect::<Vec<_>>();

        let create_info = vk::InstanceCreateInfo::builder()
            .flags(vk::InstanceCreateFlags::empty())
            .application_info(&app_info)
            .enabled_layer_names(&str_pointers[..layers.len()])
            .enabled_extension_names(&str_pointers[layers.len()..]);

        entry.create_instance(&create_info, None).map_err(|e| {
            log::warn!("create_instance: {:?}", e);
            InstanceError
        })?
    };

    vulkan::Instance::from_raw(
        entry,
        vk_instance,
        driver_api_version,
        android_sdk_version,
        extensions,
        flags,
        has_nv_optimus,
        Some(Box::new(())), // `Some` signals that wgpu-hal is in charge of destroying vk_instance
    )
}

// Modified version of wgpu-hal/src/vulkan/instance.rs
pub unsafe fn create_instance(
    desc: &native::WGPUInstanceDescriptorVK,
) -> wgpu_native::native::WGPUInstance {
    let r = createInstanceRaw(&desc);
    match r {
        Ok(instance) => {
            let context = Context::from_hal_instance::<vulkan::Api>(
                "wgpu",
                wgc::hub::IdentityManagerFactory,
                instance,
            );

            Box::into_raw(Box::new(wgpu_native::native::WGPUInstanceImpl {
                context: Arc::new(context),
            }))
        }
        Err(_) => null_mut(),
    }
}

pub unsafe fn create_adapter(
    instance: wgpu_native::native::WGPUInstance,
    options: &native::WGPURequestAdapterOptionsVK,
) -> wgpu_native::native::WGPUAdapter {
    use ash::vk::Handle;
    let instance = instance.as_ref().expect("invalid instance");
    let hal_instance = instance
        .context
        .instance_as_hal::<wgpu_hal::vulkan::Api>()
        .expect("invalid vulkan instance");
    let exposed_adapter = hal_instance
        .expose_adapter(vk::PhysicalDevice::from_raw(options.physicalDevice as u64))
        .expect("invalid adapter");

    let adapter = instance
        .context
        .create_adapter_from_hal(exposed_adapter, ());
    make_context_handle(&instance.context, adapter)
}

pub fn get_instance_properties(
    instance: wgpu_native::native::WGPUInstance,
    out: &mut native::WGPUInstancePropertiesVK,
) {
    use ash::vk::Handle;
    unsafe {
        let instance = instance.as_ref().expect("invalid instance");
        let hal_instance = instance
            .context
            .instance_as_hal::<wgpu_hal::vulkan::Api>()
            .expect("invalid vulkan instance");
        let shared = hal_instance.shared_instance();
        let entry = shared.entry();
        out.getInstanceProcAddr =
            entry.static_fn().get_instance_proc_addr as *const std::ffi::c_void;
        out.instance = shared.raw_instance().handle().as_raw() as *const std::ffi::c_void;
    }
}
