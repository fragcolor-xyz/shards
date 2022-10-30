use ash::vk::{self, Handle};
use log;
use std::ffi::{CStr, CString};
use std::hash::Hash;
use std::mem::transmute;
use std::num::NonZeroU32;
use std::ptr::null_mut;
use std::sync::Arc;
use wgc::hub::HalApi;
use wgpu_core as wgc;
use wgpu_hal::{vulkan, Device};
use wgpu_hal::{InstanceError, InstanceFlags};
use wgpu_native::conv::{
    map_extent3d, map_texture_aspect, map_texture_dimension, map_texture_format,
    map_texture_view_dimension,
};
use wgpu_native::native::{IntoHandle, IntoHandleWithContext, UnwrapId};
use wgpu_native::Context;
use wgpu_types as wgt;

use crate::native;

// Modified version of wgpu-hal/src/vulkan/instance.rs
unsafe fn create_instance_raw(
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

    let mut extensions = vulkan::Instance::required_extensions(&entry, 0, flags)?;

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
    let r = create_instance_raw(&desc);
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
        Err(_) => std::ptr::null_mut(),
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

    wgpu_native::native::WGPUAdapterImpl {
        context: instance.context.clone(),
        id: adapter,
        name: CString::default(),
        vendor_name: CString::default(),
        architecture_name: CString::default(),
        driver_desc: CString::default(),
    }
    .into_handle()
}

fn create_device_with_hal_adapter(
    adapter: &vulkan::Adapter,
    descriptor: &wgt::DeviceDescriptor<wgc::Label>,
    descriptor_vk: &native::WGPUDeviceDescriptorVK,
) -> Option<wgpu_hal::OpenDevice<vulkan::Api>> {
    let caps = adapter.physical_device_capabilities();

    // Add extensions from descriptor
    let extra_extensions: Vec<_> = unsafe {
        std::slice::from_raw_parts(
            descriptor_vk.requiredExtensions,
            descriptor_vk.requiredExtensionCount as usize,
        )
        .into_iter()
        .map(|v| CStr::from_ptr(*v))
        .collect()
    };
    let mut enabled_phd_features =
        adapter.physical_device_features(&extra_extensions, descriptor.features);

    // for ext in extra_extensions {
    //     extensions.push(ext);
    // }
    let enabled_extensions = adapter.required_device_extensions(descriptor.features);
    // let mut enabled_phd_features = adapter.physical_device_features(&enabled_extensions, features);

    let family_index = 0; //TODO
    let family_info = vk::DeviceQueueCreateInfo::builder()
        .queue_family_index(family_index)
        .queue_priorities(&[1.0])
        .build();
    let family_infos = [family_info];

    let str_pointers = enabled_extensions
        .iter()
        .map(|&s| {
            // Safe because `enabled_extensions` entries have static lifetime.
            s.as_ptr()
        })
        .collect::<Vec<_>>();

    let pre_info = vk::DeviceCreateInfo::builder()
        .queue_create_infos(&family_infos)
        .enabled_extension_names(&str_pointers);
    let info = enabled_phd_features
        .add_to_device_create_builder(pre_info)
        .build();
    let raw_device = {
        profiling::scope!("vkCreateDevice");
        unsafe {
            adapter
                .instance
                .raw
                .create_device(adapter.raw, &info, None)
                .ok()
        }
    };

    match raw_device {
        None => return None,
        Some(raw_device) => unsafe {
            adapter
                .device_from_raw(
                    raw_device,
                    true,
                    &enabled_extensions,
                    descriptor.features,
                    family_info.queue_family_index,
                    0,
                )
                .ok()
        },
    }
    // let caps = adapter.physical_device_capabilities();

    // let phd_limits = caps.properties().limits;
    // let uab_types = wgpu_hal::UpdateAfterBindTypes::from_limits(&descriptor.limits, &phd_limits);

    // let mut extensions = adapter.required_device_extensions(descriptor.features);
    // let mut enabled_phd_features =
    //     adapter.physical_device_features(&extensions, descriptor.features, uab_types);

    // // Add extensions from descriptor
    // let extra_extensions = unsafe {
    //     std::slice::from_raw_parts(
    //         descriptor_vk.requiredExtensions,
    //         descriptor_vk.requiredExtensionCount as usize,
    //     )
    //     .into_iter()
    //     .map(|v| CStr::from_ptr(*v))
    // };
    // for ext in extra_extensions {
    //     extensions.push(ext);
    // }

    // let family_index = 0; //TODO
    // let family_info = vk::DeviceQueueCreateInfo::builder()
    //     .queue_family_index(family_index)
    //     .queue_priorities(&[1.0])
    //     .build();
    // let family_infos = [family_info];

    // let str_pointers = extensions
    //     .iter()
    //     .map(|&s| {
    //         // Safe because `enabled_extensions` entries have static lifetime.
    //         s.as_ptr()
    //     })
    //     .collect::<Vec<_>>();

    // let pre_info = vk::DeviceCreateInfo::builder()
    //     .queue_create_infos(&family_infos)
    //     .enabled_extension_names(&str_pointers);
    // let info = enabled_phd_features
    //     .add_to_device_create_builder(pre_info)
    //     .build();

    // unsafe {
    //     let vk_instance = adapter.shared_instance().raw_instance();
    //     let vk_physical_device = adapter.raw_physical_device();
    //     match vk_instance.create_device(vk_physical_device, &info, None) {
    //         Ok(raw_device) => {
    //             let hal_device = adapter
    //                 .device_from_raw(
    //                     raw_device,
    //                     true,
    //                     &extensions,
    //                     descriptor.features,
    //                     uab_types,
    //                     family_info.queue_family_index,
    //                     0,
    //                 )
    //                 .unwrap();
    //             Some(hal_device)
    //         }
    //         Err(_) => None,
    //     }
    // }
}

pub unsafe fn create_device(
    context: &wgpu_native::Context,
    adapter_id: wgc::id::AdapterId,
    descriptor: &wgt::DeviceDescriptor<wgc::Label>,
    descriptor_vk: &native::WGPUDeviceDescriptorVK,
) -> Option<wgpu_hal::OpenDevice<vulkan::Api>> {
    unsafe {
        context.adapter_as_hal::<vulkan::Api, _, _>(adapter_id, |adapter| {
            create_device_with_hal_adapter(
                adapter.expect("invalid adapter"),
                &descriptor,
                &descriptor_vk,
            )
        })
    }
}

pub unsafe fn get_instance_properties(
    instance: wgpu_native::native::WGPUInstance,
    out: &mut native::WGPUInstancePropertiesVK,
) {
    use ash::vk::Handle;
    let instance = instance.as_ref().expect("invalid instance");
    let hal_instance = instance
        .context
        .instance_as_hal::<wgpu_hal::vulkan::Api>()
        .expect("invalid vulkan instance");
    let shared = hal_instance.shared_instance();
    let entry = shared.entry();
    out.getInstanceProcAddr = entry.static_fn().get_instance_proc_addr as *const _;
    out.instance = shared.raw_instance().handle().as_raw() as *const _;
}

pub unsafe fn get_device_properties(
    device: wgpu_native::native::WGPUDevice,
    out: &mut native::WGPUDevicePropertiesVK,
) {
    use ash::vk::Handle;
    let (device_id, context) = device.unwrap_handle();
    context.device_as_hal::<wgpu_hal::vulkan::Api, _, _>(device_id, |device| {
        let device = device.expect("invalid vulkan device");
        out.queueIndex = device.queue_index();
        out.queueFamilyIndex = device.queue_family_index();
        out.device = device.raw_device().handle().as_raw() as *const _;
        out.queue = device.raw_queue().as_raw() as *const _;
    });
}

pub unsafe fn create_external_texture_view(
    device: wgpu_native::native::WGPUDevice,
    texture_desc: &wgpu_native::native::WGPUTextureDescriptor,
    view_desc: &wgpu_native::native::WGPUTextureViewDescriptor,
    vulkan: &native::WGPUTextureViewDescriptorVK,
) -> wgpu_native::native::WGPUTextureView {
    let image_handle = vk::Image::from_raw(vulkan.handle as u64);

    let texture_usage =
        wgt::TextureUsages::from_bits(texture_desc.usage).expect("Invalid texture usage");
    let texture_format = map_texture_format(texture_desc.format).expect("Invalid texture format");
    let texture_size = map_extent3d(transmute(&texture_desc.size));

    let texture_desc = wgc::resource::TextureDescriptor {
        label: None,
        size: texture_size,
        mip_level_count: texture_desc.mipLevelCount,
        sample_count: texture_desc.sampleCount,
        dimension: map_texture_dimension(texture_desc.dimension),
        format: texture_format,
        usage: texture_usage,
        view_formats: Vec::new(),
    };

    let (device_id, context) = device.unwrap_handle();
    let hal_texture = context.device_as_hal::<wgpu_hal::vulkan::Api, _, _>(device_id, |device| {
        let device = device.expect("invalid vulkan device");

        let label = if view_desc.label != std::ptr::null() {
            Some(
                CStr::from_ptr(view_desc.label)
                    .to_str()
                    .expect("not a valid utf-8 string"),
            )
        } else {
            None
        };

        let texture_desc = wgpu_hal::TextureDescriptor {
            label: label,
            size: texture_desc.size,
            mip_level_count: texture_desc.mip_level_count,
            sample_count: texture_desc.sample_count,
            dimension: texture_desc.dimension,
            format: texture_desc.format,
            usage: wgc::conv::map_texture_usage(texture_desc.usage, texture_desc.format.into()),
            memory_flags: wgpu_hal::MemoryFlags::empty(),
            view_formats: Vec::new(),
        };
        vulkan::Device::texture_from_raw(image_handle, &texture_desc, Some(Box::new(())))
    });

    let (texture_id, _err) = context.create_texture_from_hal::<wgpu_hal::vulkan::Api>(
        hal_texture,
        device_id,
        &texture_desc,
        (),
    );

    // let view_desc = wgpu_hal::TextureViewDescriptor {
    //     label: label,
    //     dimension: map_texture_view_dimension(view_desc.dimension).expect("invalid texture view dimension"),
    //     format: view_format,
    //     range: wgt::ImageSubresourceRange {
    //         aspect: map_texture_aspect(view_desc.aspect),
    //         base_mip_level: view_desc.baseMipLevel,
    //         mip_level_count: NonZeroU32::new(view_desc.mipLevelCount),
    //         base_array_layer: view_desc.baseArrayLayer,
    //         array_layer_count: NonZeroU32::new(view_desc.arrayLayerCount),
    //     },
    // };
    // device.create_texture_view(&texture, &desc)

    let view_desc = wgc::resource::TextureViewDescriptor {
        label: None,
        dimension: map_texture_view_dimension(view_desc.dimension),
        format: map_texture_format(view_desc.format),
        range: wgt::ImageSubresourceRange {
            aspect: map_texture_aspect(view_desc.aspect),
            base_mip_level: view_desc.baseMipLevel,
            mip_level_count: Some(view_desc.mipLevelCount),
            base_array_layer: view_desc.baseArrayLayer,
            array_layer_count: Some(view_desc.arrayLayerCount),
        },
    };

    let (view_id, _err) =
        context.texture_create_view::<wgpu_hal::vulkan::Api>(texture_id, &view_desc, ());

    view_id.into_handle_with_context(context)
}

#[no_mangle]
pub unsafe extern "C" fn wgpuPrepareExternalPresentVK(
    queue: wgpu_native::native::WGPUQueue,
    present: *mut native::WGPUExternalPresentVK,
) {
    let present = present.as_mut().expect("invalid parameter");

    let (device_id, context) = queue.unwrap_handle();

    context.device_and_queue_as_hal_mut::<wgpu_hal::vulkan::Api, _, _>(device_id, |tuple| {
        let (_device, queue) = tuple.expect("invalid device");
        present.waitSemaphore = match queue.prepare_present_semaphore() {
            Some(semaphore) => semaphore.as_raw() as *mut _,
            None => null_mut(),
        };
    });
}
