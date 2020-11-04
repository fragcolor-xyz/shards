use crate::chainblocksc::CBContext;
use crate::chainblocksc::CBExposedTypesInfo;
use crate::chainblocksc::CBInstanceData;
use crate::chainblocksc::CBParameterInfo;
use crate::chainblocksc::CBParametersInfo;
use crate::chainblocksc::CBSeq;
use crate::chainblocksc::CBTable;
use crate::chainblocksc::CBTypeInfo;
use crate::chainblocksc::CBTypesInfo;
use crate::chainblocksc::CBVar;
use crate::chainblocksc::CBlock;
use crate::chainblocksc::CBlockPtr;
use crate::core::abortChain;
use crate::core::Core;
use crate::types::Chain;
use crate::types::ComposeResult;
use crate::types::Context;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::ParameterInfoView;
use crate::types::Parameters;
use crate::types::Table;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use core::convert::TryInto;
use core::result::Result;
use core::slice;
use std::ffi::CStr;
use std::ffi::CString;
use std::os::raw::c_char;

pub trait Block {
    fn registerName() -> &'static str;

    fn name(&mut self) -> &str;
    fn help(&mut self) -> &str {
        ""
    }

    fn setup(&mut self) {}
    fn destroy(&mut self) {}

    fn inputTypes(&mut self) -> &Types;
    fn outputTypes(&mut self) -> &Types;

    fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
        None
    }
    fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
        None
    }

    fn hasCompose() -> bool {
        false
    }
    fn compose(&mut self, _data: &InstanceData) -> Result<Type, &str> {
        Ok(Type::default())
    }

    fn hasComposed() -> bool {
        false
    }
    fn composed(&mut self, _chain: &Chain, _results: &ComposeResult) {}

    fn parameters(&mut self) -> Option<&Parameters> {
        None
    }
    fn setParam(&mut self, _index: i32, _value: &Var) {}
    fn getParam(&mut self, _index: i32) -> Var {
        Var::default()
    }

    fn warmup(&mut self, _context: &Context) -> Result<(), &str> {
        Ok(())
    }
    fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str>;
    fn cleanup(&mut self) {}

    fn hasMutate() -> bool {
        false
    }
    fn mutate(&mut self, _options: Table) {}

    fn hasCrossover() -> bool {
        false
    }
    fn crossover(&mut self, _state0: &Var, _state1: &Var) {}

    fn hasState() -> bool {
        false
    }
    fn getState(&mut self) -> Var {
        Var::default()
    }
    fn setState(&mut self, _state: &Var) {}
    fn resetState(&mut self) {}
}

#[repr(C)]
pub struct BlockWrapper<T: Block> {
    header: CBlock,
    pub block: T,
    name: Option<CString>,
    help: Option<CString>,
}

pub unsafe extern "C" fn cblock_construct<T: Default + Block>() -> *mut CBlock {
    let wrapper: Box<BlockWrapper<T>> = Box::new(create());
    let wptr = Box::into_raw(wrapper);
    wptr as *mut CBlock
}

unsafe extern "C" fn cblock_name<T: Block>(arg1: *mut CBlock) -> *const ::std::os::raw::c_char {
    let blk = arg1 as *mut BlockWrapper<T>;
    let name = (*blk).block.name();
    (*blk).name = Some(CString::new(name).expect("CString::new failed"));
    (*blk).name.as_ref().unwrap().as_ptr()
}

unsafe extern "C" fn cblock_help<T: Block>(arg1: *mut CBlock) -> *const ::std::os::raw::c_char {
    let blk = arg1 as *mut BlockWrapper<T>;
    let help = (*blk).block.help();
    (*blk).help = Some(CString::new(help).expect("CString::new failed"));
    (*blk).help.as_ref().unwrap().as_ptr()
}

unsafe extern "C" fn cblock_inputTypes<T: Block>(arg1: *mut CBlock) -> CBTypesInfo {
    let blk = arg1 as *mut BlockWrapper<T>;
    let t = (*blk).block.inputTypes();
    CBTypesInfo::from(t)
}

unsafe extern "C" fn cblock_outputTypes<T: Block>(arg1: *mut CBlock) -> CBTypesInfo {
    let blk = arg1 as *mut BlockWrapper<T>;
    let t = (*blk).block.outputTypes();
    CBTypesInfo::from(t)
}

unsafe extern "C" fn cblock_setup<T: Block>(arg1: *mut CBlock) {
    let blk = arg1 as *mut BlockWrapper<T>;
    (*blk).block.setup();
}

unsafe extern "C" fn cblock_destroy<T: Block>(arg1: *mut CBlock) {
    let blk = arg1 as *mut BlockWrapper<T>;
    (*blk).block.destroy();
    Box::from_raw(blk);
    drop(blk);
}

unsafe extern "C" fn cblock_warmup<T: Block>(arg1: *mut CBlock, arg2: *mut CBContext) {
    let blk = arg1 as *mut BlockWrapper<T>;
    if let Err(error) = (*blk).block.warmup(&(*arg2)) {
        abortChain(&(*arg2), error);
    }
}

unsafe extern "C" fn cblock_activate<T: Block>(
    arg1: *mut CBlock,
    arg2: *mut CBContext,
    arg3: *const CBVar,
) -> CBVar {
    let blk = arg1 as *mut BlockWrapper<T>;
    match (*blk).block.activate(&(*arg2), &(*arg3)) {
        Ok(value) => value,
        Err(error) => {
            abortChain(&(*arg2), error);
            Var::default()
        }
    }
}

unsafe extern "C" fn cblock_mutate<T: Block>(arg1: *mut CBlock, arg2: CBTable) {
    let blk = arg1 as *mut BlockWrapper<T>;
    (*blk).block.mutate(arg2.into());
}

unsafe extern "C" fn cblock_cleanup<T: Block>(arg1: *mut CBlock) {
    let blk = arg1 as *mut BlockWrapper<T>;
    (*blk).block.cleanup();
}

unsafe extern "C" fn cblock_exposedVariables<T: Block>(arg1: *mut CBlock) -> CBExposedTypesInfo {
    let blk = arg1 as *mut BlockWrapper<T>;
    if let Some(exposed) = (*blk).block.exposedVariables() {
        CBExposedTypesInfo::from(exposed)
    } else {
        CBExposedTypesInfo::default()
    }
}

unsafe extern "C" fn cblock_requiredVariables<T: Block>(arg1: *mut CBlock) -> CBExposedTypesInfo {
    let blk = arg1 as *mut BlockWrapper<T>;
    if let Some(required) = (*blk).block.requiredVariables() {
        CBExposedTypesInfo::from(required)
    } else {
        CBExposedTypesInfo::default()
    }
}

unsafe extern "C" fn cblock_compose<T: Block>(
    arg1: *mut CBlock,
    data: CBInstanceData,
) -> CBTypeInfo {
    let blk = arg1 as *mut BlockWrapper<T>;
    match (*blk).block.compose(&data) {
        Ok(output) => output,
        Err(error) => {
            let cmsg = CString::new(error).unwrap();
            data.reportError.unwrap()(data.privateContext, cmsg.as_ptr(), false);
            CBTypeInfo::default()
        }
    }
}

unsafe extern "C" fn cblock_composed<T: Block>(
    arg1: *mut CBlock,
    chain: *const Chain,
    results: *const ComposeResult,
) {
    let blk = arg1 as *mut BlockWrapper<T>;
    (*blk).block.composed(&(*chain), &(*results));
}

unsafe extern "C" fn cblock_parameters<T: Block>(arg1: *mut CBlock) -> CBParametersInfo {
    let blk = arg1 as *mut BlockWrapper<T>;
    if let Some(params) = (*blk).block.parameters() {
        CBParametersInfo::from(params)
    } else {
        CBParametersInfo::default()
    }
}

unsafe extern "C" fn cblock_getParam<T: Block>(
    arg1: *mut CBlock,
    arg2: ::std::os::raw::c_int,
) -> CBVar {
    let blk = arg1 as *mut BlockWrapper<T>;
    (*blk).block.getParam(arg2)
}

unsafe extern "C" fn cblock_setParam<T: Block>(
    arg1: *mut CBlock,
    arg2: ::std::os::raw::c_int,
    arg3: CBVar,
) {
    let blk = arg1 as *mut BlockWrapper<T>;
    (*blk).block.setParam(arg2, &arg3);
}

unsafe extern "C" fn cblock_crossover<T: Block>(arg1: *mut CBlock, s0: Var, s1: Var) {
    let blk = arg1 as *mut BlockWrapper<T>;
    (*blk).block.crossover(&s0, &s1);
}

unsafe extern "C" fn cblock_getState<T: Block>(arg1: *mut CBlock) -> Var {
    let blk = arg1 as *mut BlockWrapper<T>;
    (*blk).block.getState()
}

unsafe extern "C" fn cblock_setState<T: Block>(arg1: *mut CBlock, state: Var) {
    let blk = arg1 as *mut BlockWrapper<T>;
    (*blk).block.setState(&state);
}

unsafe extern "C" fn cblock_resetState<T: Block>(arg1: *mut CBlock) {
    let blk = arg1 as *mut BlockWrapper<T>;
    (*blk).block.resetState();
}

pub fn create<T: Default + Block>() -> BlockWrapper<T> {
    BlockWrapper::<T> {
        header: CBlock {
            inlineBlockId: 0,
            owned: false,
            name: Some(cblock_name::<T>),
            help: Some(cblock_help::<T>),
            inputTypes: Some(cblock_inputTypes::<T>),
            outputTypes: Some(cblock_outputTypes::<T>),
            setup: Some(cblock_setup::<T>),
            destroy: Some(cblock_destroy::<T>),
            exposedVariables: Some(cblock_exposedVariables::<T>),
            requiredVariables: Some(cblock_requiredVariables::<T>),
            compose: if T::hasCompose() {
                Some(cblock_compose::<T>)
            } else {
                None
            },
            composed: if T::hasComposed() {
                Some(cblock_composed::<T>)
            } else {
                None
            },
            parameters: Some(cblock_parameters::<T>),
            setParam: Some(cblock_setParam::<T>),
            getParam: Some(cblock_getParam::<T>),
            warmup: Some(cblock_warmup::<T>),
            activate: Some(cblock_activate::<T>),
            cleanup: Some(cblock_cleanup::<T>),
            mutate: if T::hasMutate() {
                Some(cblock_mutate::<T>)
            } else {
                None
            },
            crossover: if T::hasCrossover() {
                Some(cblock_crossover::<T>)
            } else {
                None
            },
            getState: if T::hasState() {
                Some(cblock_getState::<T>)
            } else {
                None
            },
            setState: if T::hasState() {
                Some(cblock_setState::<T>)
            } else {
                None
            },
            resetState: if T::hasState() {
                Some(cblock_resetState::<T>)
            } else {
                None
            },
        },
        block: T::default(),
        name: None,
        help: None,
    }
}
