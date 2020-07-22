use std::os::raw::c_char;
use crate::chainblocksc::CBBool;
use crate::block::cblock_construct;
use crate::block::Block;
use crate::chainblocksc::chainblocksInterface;
use crate::chainblocksc::CBContext;
use crate::chainblocksc::CBCore;
use crate::chainblocksc::CBString;
use crate::chainblocksc::CBVar;
use crate::chainblocksc::CBlockPtr;
use crate::types::Var;
use std::ffi::CStr;
use std::ffi::CString;

const ABI_VERSION: u32 = 0x20200101;

extern crate dlopen;
use dlopen::symbor::Library;

fn try_load_dlls() -> Option<Library> {
    if let Ok(lib) = Library::open("libcb.dylib") {
        Some(lib)
    } else if let Ok(lib) = Library::open("libcb_shared.dylib") {
        Some(lib)
    } else if let Ok(lib) = Library::open("libcb.so") {
        Some(lib)
    } else if let Ok(lib) = Library::open("libcb_shared.so") {
        Some(lib)
    } else if let Ok(lib) = Library::open("libcb.dll") {
        Some(lib)
    } else {
        None
    }
}

pub static mut CBDLL: Option<Library> = None;

pub static mut Core: CBCore = CBCore {
    registerBlock: None,
    registerObjectType: None,
    registerEnumType: None,
    registerRunLoopCallback: None,
    unregisterRunLoopCallback: None,
    registerExitCallback: None,
    unregisterExitCallback: None,
    referenceVariable: None,
    referenceChainVariable: None,
    releaseVariable: None,
    abortChain: None,
    suspend: None,
    cloneVar: None,
    destroyVar: None,
    seqFree: None,
    seqPush: None,
    seqInsert: None,
    seqPop: None,
    seqResize: None,
    seqFastDelete: None,
    seqSlowDelete: None,
    typesFree: None,
    typesPush: None,
    typesInsert: None,
    typesPop: None,
    typesResize: None,
    typesFastDelete: None,
    typesSlowDelete: None,
    paramsFree: None,
    paramsPush: None,
    paramsInsert: None,
    paramsPop: None,
    paramsResize: None,
    paramsFastDelete: None,
    paramsSlowDelete: None,
    blocksFree: None,
    blocksPush: None,
    blocksInsert: None,
    blocksPop: None,
    blocksResize: None,
    blocksFastDelete: None,
    blocksSlowDelete: None,
    expTypesFree: None,
    expTypesPush: None,
    expTypesInsert: None,
    expTypesPop: None,
    expTypesResize: None,
    expTypesFastDelete: None,
    expTypesSlowDelete: None,
    stringsFree: None,
    stringsPush: None,
    stringsInsert: None,
    stringsPop: None,
    stringsResize: None,
    stringsFastDelete: None,
    stringsSlowDelete: None,
    tableNew: None,
    composeChain: None,
    validateSetParam: None,
    runChain: None,
    composeBlocks: None,
    runBlocks: None,
    getChainInfo: None,
    log: None,
    setLoggingOptions: None,
    createBlock: None,
    createChain: None,
    setChainName: None,
    setChainLooped: None,
    setChainUnsafe: None,
    addBlock: None,
    removeBlock: None,
    destroyChain: None,
    stopChain: None,
    createNode: None,
    destroyNode: None,
    schedule: None,
    unschedule: None,
    tick: None,
    sleep: None,
    getRootPath: None,
    setRootPath: None,
};

static mut init_done: bool = false;

unsafe fn initInternal() {
    let exe = Library::open_self().ok().unwrap();

    let exefun = exe
        .symbol::<unsafe extern "C" fn(abi_version: u32, pcore: *mut CBCore) -> CBBool>("chainblocksInterface")
        .ok();
    if exefun.is_some() {
        let fun = exefun.unwrap();
        let res = fun(ABI_VERSION, &mut Core);
        if !res {
            panic!("Failed to aquire chainblocks interface, version not compatible.");
        }
        log("chainblocks-rs attached! (exe)");
    } else {
        let lib = try_load_dlls().unwrap();
        let fun = lib
            .symbol::<unsafe extern "C" fn(abi_version: u32, pcore: *mut CBCore) -> CBBool>("chainblocksInterface")
            .unwrap();
        let res = fun(ABI_VERSION, &mut Core);
        if !res {
            panic!("Failed to aquire chainblocks interface, version not compatible.");
        }
        CBDLL = Some(lib);
        log("chainblocks-rs attached! (dll)");
    }
    init_done = true;
}

#[inline(always)]
pub fn init() {
    unsafe {
        if !init_done {
            initInternal();
        }
    }
}

#[inline(always)]
pub fn log(s: &str) {
    let clog = CString::new(s).unwrap();
    unsafe {
        Core.log.unwrap()(clog.as_ptr());
    }
}

#[inline(always)]
pub fn sleep(seconds: f64) {
    unsafe {
        Core.sleep.unwrap()(seconds, true);
    }
}

#[inline(always)]
pub fn suspend(context: &CBContext, seconds: f64) {
    unsafe {
        let ctx = context as *const CBContext as *mut CBContext;
        Core.suspend.unwrap()(ctx, seconds);
    }
}

#[inline(always)]
pub fn abortChain(context: &CBContext, message: &str) {
    let cmsg = CString::new(message).unwrap();
    unsafe {
        let ctx = context as *const CBContext as *mut CBContext;
        Core.abortChain.unwrap()(ctx, cmsg.as_ptr());
    }
}

#[inline(always)]
pub fn registerBlock<T: Default + Block>() {
    unsafe {
        Core.registerBlock.unwrap()(T::registerName().as_ptr() as *const c_char , Some(cblock_construct::<T>));
    }
}

#[inline(always)]
pub fn getRootPath() -> &'static str {
    unsafe {
        CStr::from_ptr(Core.getRootPath.unwrap()())
            .to_str()
            .unwrap()
    }
}

#[inline(always)]
pub fn createBlock(name: &str) -> CBlockPtr {
    let cname = CString::new(name).unwrap();
    unsafe { Core.createBlock.unwrap()(cname.as_ptr()) }
}

#[inline(always)]
pub fn cloneVar(dst: &mut Var, src: &Var) {
    unsafe {
        Core.cloneVar.unwrap()(dst, src);
    }
}

pub fn referenceMutVariable(context: &CBContext, name: CBString) -> &mut CBVar {
    unsafe {
        let ctx = context as *const CBContext as *mut CBContext;
        let cbptr = Core.referenceVariable.unwrap()(ctx, name);
        cbptr.as_mut().unwrap()
    }
}

pub fn referenceVariable(context: &CBContext, name: CBString) -> &CBVar {
    unsafe {
        let ctx = context as *const CBContext as *mut CBContext;
        let cbptr = Core.referenceVariable.unwrap()(ctx, name);
        cbptr.as_mut().unwrap()
    }
}

pub fn releaseMutVariable(var: &mut CBVar) {
     unsafe {
        let v = var as *mut CBVar;
        Core.releaseVariable.unwrap()(v);
    }
}

pub fn releaseVariable(var: &CBVar) {
    unsafe {
        let v = var as *const CBVar as *mut CBVar;
        Core.releaseVariable.unwrap()(v);
    }
}
