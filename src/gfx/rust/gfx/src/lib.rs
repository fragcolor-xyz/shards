#[cfg(feature = "wgpu")]
extern crate wgpu_native;

pub mod native {
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(dead_code)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

#[cfg(all(feature = "wgpu", feature = "vulkan"))]
mod vulkan;

// TODO: Try to unify with the existing macro
#[macro_export]
macro_rules! follow_chain {
    ($func:ident($base:expr $(, $stype:ident => $ty:ty)*)) => {{
    #[allow(non_snake_case)] // We use the type name as an easily usable temporary name
    {
        $(
            let mut $stype: Option<&$ty> = None;
        )*
        let mut chain_opt: Option<&$crate::wgpu_native::native::WGPUChainedStruct> = $base.nextInChain.as_ref();
        while let Some(next_in_chain) = chain_opt {
            match next_in_chain.sType {
                $(
                    $crate::native::$stype => {
                        let next_in_chain_ptr = next_in_chain as *const $crate::wgpu_native::native::WGPUChainedStruct;
                        assert_eq!(
                            0,
                            next_in_chain_ptr.align_offset(::std::mem::align_of::<$ty>()),
                            concat!("Chain structure pointer is not aligned correctly to dereference as ", stringify!($ty), ". Correct alignment: {}"),
                            ::std::mem::align_of::<$ty>()
                        );
                        let type_ptr: *const $ty = next_in_chain_ptr as _;
                        $stype = Some(&*type_ptr);
                    }
                )*
                _ => {}
            }
            chain_opt = next_in_chain.next.as_ref();
        }
        $func($base, $($stype),*)
    }}};
}

#[macro_export]
macro_rules! follow_chain1 {
    ($base:expr $(, {$chain_type:pat, $struct_type:ty} => $cb:expr )*) => {{
        let mut chain_opt = $base.nextInChain.as_ref();
        while let Some(next_in_chain) = chain_opt {
            match next_in_chain.sType {
                $(
                    $chain_type => {
                        let ptr = next_in_chain as *const _;
                        let next_in_chain_ptr = ptr as *const $crate::wgpu_native::native::WGPUChainedStruct;
                        assert_eq!(
                            0,
                            next_in_chain_ptr.align_offset(::std::mem::align_of::<$struct_type>()),
                            concat!("Chain structure pointer is not aligned correctly to dereference as ", stringify!($struct_type), ". Correct alignment: {}"),
                            ::std::mem::align_of::<$struct_type>()
                        );
                        let type_ptr = next_in_chain_ptr as *const $struct_type;
                        $cb(&*type_ptr);
                    }
                )*
                _ => {}
            }
            chain_opt = next_in_chain.next.as_mut();
        }
    }};
    (mut $base:expr $(, {$chain_type:pat, $struct_type:ty} => $cb:expr )*) => {{
        let mut chain_opt = $base.nextInChain.as_mut();
        while let Some(next_in_chain) = chain_opt {
            match next_in_chain.sType {
                $(
                    $chain_type => {
                        let ptr = next_in_chain as *mut _;
                        let next_in_chain_ptr = ptr as *mut $crate::wgpu_native::native::WGPUChainedStructOut;
                        assert_eq!(
                            0,
                            next_in_chain_ptr.align_offset(::std::mem::align_of::<$struct_type>()),
                            concat!("Chain structure pointer is not aligned correctly to dereference as ", stringify!($struct_type), ". Correct alignment: {}"),
                            ::std::mem::align_of::<$struct_type>()
                        );
                        let type_ptr = next_in_chain_ptr as *mut $struct_type;
                        $cb(&mut *type_ptr);
                    }
                )*
                _ => {}
            }
            chain_opt = next_in_chain.next.as_mut();
        }
    }};
}

enum CreateInstanceParams<'a> {
    Default,
    Vulkan(&'a native::WGPUInstanceDescriptorVK),
}

fn map_instance_descriptor<'a>(
    _desc: &wgpu_native::native::WGPUInstanceDescriptor,
    vulkan: Option<&'a native::WGPUInstanceDescriptorVK>,
) -> CreateInstanceParams<'a> {
    if let Some(vulkan) = vulkan {
        CreateInstanceParams::Vulkan(vulkan)
    } else {
        CreateInstanceParams::Default
    }
}

#[no_mangle]
pub unsafe extern "C" fn wgpuCreateInstanceEx(
    descriptor: *const wgpu_native::native::WGPUInstanceDescriptor,
) -> wgpu_native::native::WGPUInstance {
    let create_instance_params = follow_chain!(
        map_instance_descriptor(descriptor.as_ref().unwrap(),
            WGPUNativeSTypeEx_InstanceDescriptorVK => native::WGPUInstanceDescriptorVK)
    );

    match create_instance_params {
        CreateInstanceParams::Vulkan(desc) => vulkan::create_instance(desc),
        CreateInstanceParams::Default => wgpu_native::wgpuCreateInstance(descriptor),
    }
}

#[no_mangle]
pub unsafe extern "C" fn wgpuInstanceGetPropertiesEx(
    instance: wgpu_native::native::WGPUInstance,
    out: *mut native::WGPUInstanceProperties,
) {
    follow_chain1!(mut out.as_mut().unwrap(),
        {native::WGPUNativeSTypeEx_InstancePropertiesVK, native::WGPUInstancePropertiesVK}  => |v: &mut native::WGPUInstancePropertiesVK|{
            vulkan::get_instance_properties(instance, v);
        }
    );
}

enum AdapterRequest<'a> {
    Default(&'a wgpu_native::native::WGPURequestAdapterOptions),
    Vulkan(&'a native::WGPURequestAdapterOptionsVK),
}

fn map_adapter_request<'a>(
    _desc: &'a wgpu_native::native::WGPURequestAdapterOptions,
    vulkan: Option<&'a native::WGPURequestAdapterOptionsVK>,
) -> AdapterRequest<'a> {
    if let Some(vulkan) = vulkan {
        AdapterRequest::Vulkan(vulkan)
    } else {
        AdapterRequest::Default(&_desc)
    }
}

#[no_mangle]
pub unsafe extern "C" fn wgpuInstanceRequestAdapterEx(
    instance: wgpu_native::native::WGPUInstance,
    options: &wgpu_native::native::WGPURequestAdapterOptions,
    callback: wgpu_native::native::WGPURequestAdapterCallback,
    userdata: *mut std::os::raw::c_void,
) {

    let request = follow_chain!(
        map_adapter_request(options,
            WGPUNativeSTypeEx_RequestAdapterOptionsVK => native::WGPURequestAdapterOptionsVK)
    );

    match request {
        AdapterRequest::Vulkan(options) => {
            let adapter = vulkan::create_adapter(instance, &options);
            // TODO: Handle errors
            (callback.unwrap())(
                wgpu_native::native::WGPURequestAdapterStatus_Success,
                adapter,
                std::ptr::null(),
                userdata
            );
        }
        AdapterRequest::Default(options) => wgpu_native::device::wgpuInstanceRequestAdapter(instance, &options, callback, userdata),
    }
}


// enum CreateDeviceParams<'a> {
//     Default,
//     Vulkan(&'a native::WGPUDeviceDescriptorVK),
// }

// fn map_device_descriptor<'a>(
//     _desc: &wgpu_native::native::WGPUDeviceDescriptor,
//     vulkan: Option<&'a native::WGPUDeviceDescriptorVK>,
// ) -> CreateDeviceParams<'a> {
//     if let Some(vulkan) = vulkan {
//         CreateDeviceParams::Vulkan(vulkan)
//     } else {
//         CreateDeviceParams::Default
//     }
// }

// #[no_mangle]
// pub unsafe extern "C" fn wgpuCreateDeviceEx(
//     descriptor: *const wgpu_native::native::WGPUDeviceDescriptor,
// ) -> wgpu_native::native::WGPUDevice {
//     let create_device_params = follow_chain!(
//         map_device_descriptor(descriptor.as_ref().unwrap(),
//             WGPUNativeSTypeEx_DeviceDescriptorVK => native::WGPUDeviceDescriptorVK)
//     );

//     match create_device_params {
//         CreateDeviceParams::Vulkan(desc) => vulkan::create_device(desc),
//         CreateDeviceParams::Default => wgpu_native::wgpuCreateDevice(descriptor),
//     }
// }
