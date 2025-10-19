/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

use rustpython_vm as vm;
use rustpython_vm::{Interpreter, PyObjectRef, PyResult, VirtualMachine, AsObject};
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::common_type;
use shards::types::{
    ClonedVar, Context, ExposedTypes, InstanceData, ParamVar,
    Type, Types, Var, BOOL_TYPES, STRING_TYPES,
};

lazy_static! {
    static ref ANY_TYPES: Vec<Type> = vec![common_type::any];
}

/// Convert SHVar to RustPython PyObject
fn shvar_to_py(vm: &VirtualMachine, var: &Var) -> PyResult<PyObjectRef> {
    use shards::shardsc::{SHType_None, SHType_Bool, SHType_Int, SHType_Float, SHType_String, SHType_Bytes,
                          SHType_Int2, SHType_Int3, SHType_Int4, SHType_Float2, SHType_Float3, SHType_Float4};

    match var.valueType {
        SHType_None => Ok(vm.ctx.none()),
        SHType_Bool => Ok(vm.ctx.new_bool(unsafe { var.payload.__bindgen_anon_1.boolValue }).into()),
        SHType_Int => Ok(vm.ctx.new_int(unsafe { var.payload.__bindgen_anon_1.intValue }).into()),
        SHType_Float => Ok(vm.ctx.new_float(unsafe { var.payload.__bindgen_anon_1.floatValue }).into()),
        SHType_String => {
            let str_val: &str = var.try_into().map_err(|e| {
                vm.new_runtime_error(format!("Failed to convert string: {}", e))
            })?;
            Ok(vm.ctx.new_str(str_val).into())
        }
        SHType_Bytes => {
            // Convert Var to bytes slice
            let bytes: &[u8] = var.try_into().map_err(|e| {
                vm.new_runtime_error(format!("Failed to convert bytes: {}", e))
            })?;
            Ok(vm.ctx.new_bytes(bytes.to_vec()).into())
        }
        SHType_Int2 => {
            let vals = unsafe { var.payload.__bindgen_anon_1.int2Value };
            Ok(vm.ctx.new_tuple(vec![vm.ctx.new_int(vals[0]).into(), vm.ctx.new_int(vals[1]).into()]).into())
        }
        SHType_Int3 => {
            let vals = unsafe { var.payload.__bindgen_anon_1.int3Value };
            Ok(vm.ctx.new_tuple(vec![
                vm.ctx.new_int(vals[0]).into(),
                vm.ctx.new_int(vals[1]).into(),
                vm.ctx.new_int(vals[2]).into(),
            ]).into())
        }
        SHType_Int4 => {
            let vals = unsafe { var.payload.__bindgen_anon_1.int4Value };
            Ok(vm.ctx.new_tuple(vec![
                vm.ctx.new_int(vals[0]).into(),
                vm.ctx.new_int(vals[1]).into(),
                vm.ctx.new_int(vals[2]).into(),
                vm.ctx.new_int(vals[3]).into(),
            ]).into())
        }
        SHType_Float2 => {
            let vals = unsafe { var.payload.__bindgen_anon_1.float2Value };
            Ok(vm.ctx.new_tuple(vec![vm.ctx.new_float(vals[0] as f64).into(), vm.ctx.new_float(vals[1] as f64).into()]).into())
        }
        SHType_Float3 => {
            let vals = unsafe { var.payload.__bindgen_anon_1.float3Value };
            Ok(vm.ctx.new_tuple(vec![
                vm.ctx.new_float(vals[0] as f64).into(),
                vm.ctx.new_float(vals[1] as f64).into(),
                vm.ctx.new_float(vals[2] as f64).into(),
            ]).into())
        }
        SHType_Float4 => {
            let vals = unsafe { var.payload.__bindgen_anon_1.float4Value };
            Ok(vm.ctx.new_tuple(vec![
                vm.ctx.new_float(vals[0] as f64).into(),
                vm.ctx.new_float(vals[1] as f64).into(),
                vm.ctx.new_float(vals[2] as f64).into(),
                vm.ctx.new_float(vals[3] as f64).into(),
            ]).into())
        }
        _ => Err(vm.new_runtime_error(format!(
            "Unsupported SHVar type for conversion: {:?}",
            var.valueType
        ))),
    }
}

/// Convert RustPython PyObject to SHVar
fn py_to_shvar(vm: &VirtualMachine, obj: PyObjectRef, output: &mut ClonedVar) -> Result<(), String> {
    // Check for None
    if vm.is_none(&obj) {
        *output = ClonedVar::default();
        return Ok(());
    }

    // Check for bool (must be before int, as bool is subclass of int in Python)
    if obj.class().is(vm.ctx.types.bool_type.as_object()) {
        let bool_val = obj.try_to_bool(vm).map_err(|e| format!("Bool conversion failed: {:?}", e))?;
        *output = Var::new_bool(bool_val).into();
        return Ok(());
    }

    // Check for int
    if obj.class().is(vm.ctx.types.int_type.as_object()) {
        let int_val = obj
            .try_to_value::<i64>(vm)
            .map_err(|e| format!("Int conversion failed: {:?}", e))?;
        *output = int_val.into();
        return Ok(());
    }

    // Check for float
    if obj.class().is(vm.ctx.types.float_type.as_object()) {
        use rustpython_vm::builtins::PyFloat;
        let py_float = obj.downcast::<PyFloat>()
            .map_err(|_| "Failed to downcast to PyFloat")?;
        let float_val = py_float.to_f64();
        *output = float_val.into();
        return Ok(());
    }

    // Check for string
    if obj.class().is(vm.ctx.types.str_type.as_object()) {
        let str_val = obj
            .try_to_value::<String>(vm)
            .map_err(|e| format!("String conversion failed: {:?}", e))?;
        *output = Var::ephemeral_string(&str_val).into();
        return Ok(());
    }

    // Check for bytes
    if obj.class().is(vm.ctx.types.bytes_type.as_object()) {
        let bytes_val = obj
            .try_to_value::<Vec<u8>>(vm)
            .map_err(|e| format!("Bytes conversion failed: {:?}", e))?;
        *output = Var::ephemeral_slice(&bytes_val).into();
        return Ok(());
    }

    // Check for tuple
    if obj.class().is(vm.ctx.types.tuple_type.as_object()) {
        use rustpython_vm::builtins::PyTuple;
        let tuple = obj.clone()
            .downcast::<PyTuple>()
            .map_err(|_| "Failed to downcast to tuple")?;
        let elements = tuple.as_slice();

        // Try to convert to vector types (Int2-4, Float2-4)
        if elements.len() >= 2 && elements.len() <= 4 {
            // Check if all elements are ints
            if elements.iter().all(|e| e.class().is(vm.ctx.types.int_type.as_object())) {
                let ints: Result<Vec<i64>, _> = elements.iter().map(|e| e.try_to_value::<i64>(vm)).collect();
                if let Ok(vals) = ints {
                    match vals.len() {
                        2 => {
                            *output = Var::new_int2(vals[0], vals[1]).into();
                            return Ok(());
                        }
                        3 => {
                            *output = Var::new_int3(vals[0] as i32, vals[1] as i32, vals[2] as i32).into();
                            return Ok(());
                        }
                        4 => {
                            *output = Var::new_int4(vals[0] as i32, vals[1] as i32, vals[2] as i32, vals[3] as i32).into();
                            return Ok(());
                        }
                        _ => {}
                    }
                }
            }

            // Check if all elements are floats (or ints that can be converted to floats)
            if elements.iter().all(|e| {
                e.class().is(vm.ctx.types.float_type.as_object()) || e.class().is(vm.ctx.types.int_type.as_object())
            }) {
                // Manually convert each element to f64
                let mut floats: Vec<f64> = Vec::new();
                for e in elements {
                    if e.class().is(vm.ctx.types.float_type.as_object()) {
                        // For float types, downcast to PyFloat and extract value
                        use rustpython_vm::builtins::PyFloat;
                        let py_float = e.clone().downcast::<PyFloat>().map_err(|_| "Failed to downcast to PyFloat")?;
                        floats.push(py_float.to_f64());
                    } else if e.class().is(vm.ctx.types.int_type.as_object()) {
                        let i: i64 = e.try_to_value::<i64>(vm).map_err(|e| format!("Int conversion failed: {:?}", e))?;
                        floats.push(i as f64);
                    }
                }

                match floats.len() {
                    2 => {
                        *output = Var::new_float2(floats[0], floats[1]).into();
                        return Ok(());
                    }
                    3 => {
                        *output = Var::new_float3(floats[0] as f32, floats[1] as f32, floats[2] as f32).into();
                        return Ok(());
                    }
                    4 => {
                        *output = Var::new_float4(floats[0] as f32, floats[1] as f32, floats[2] as f32, floats[3] as f32).into();
                        return Ok(());
                    }
                    _ => {}
                }
            }
        }
    }

    Err(format!(
        "Unsupported Python type for conversion to SHVar: {}",
        obj.class().name()
    ))
}

#[derive(shards::shard)]
#[shard_info(
    "Py.Eval",
    "Evaluates Python expressions using embedded RustPython interpreter"
)]
struct PyEvalShard {
    #[shard_required]
    required: ExposedTypes,

    #[shard_param(
        "Expression",
        "Python code to evaluate. Input is available as _s. The value of the last expression is returned.",
        STRING_TYPES
    )]
    expression: ParamVar,

    #[shard_param(
        "PreserveState",
        "Whether to preserve variable state between calls",
        BOOL_TYPES
    )]
    preserve_state: ParamVar,

    #[shard_param(
        "FileMode",
        "Execute as file instead of as expression (useful for multi-line scripts)",
        BOOL_TYPES
    )]
    file_mode: ParamVar,

    // Internal state
    interpreter: Option<Interpreter>,
    compiled_code: Option<PyObjectRef>,  // Stores PyRef<PyCode>
    locals: Option<PyObjectRef>,
    globals: Option<PyObjectRef>,
    output: ClonedVar,
}

impl Default for PyEvalShard {
    fn default() -> Self {
        Self {
            required: ExposedTypes::new(),
            expression: ParamVar::default(),
            preserve_state: ParamVar::new(Var::new_bool(false)),
            file_mode: ParamVar::new(Var::new_bool(false)),
            interpreter: None,
            compiled_code: None,
            locals: None,
            globals: None,
            output: ClonedVar::default(),
        }
    }
}

#[shards::shard_impl]
impl Shard for PyEvalShard {
    fn input_types(&mut self) -> &Types {
        &ANY_TYPES
    }

    fn output_types(&mut self) -> &Types {
        &ANY_TYPES
    }

    fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
        self.warmup_helper(ctx)?;

        // Create RustPython interpreter instance
        let interp = Interpreter::with_init(Default::default(), |vm| {
            // Initialize with frozen stdlib
            vm.add_frozen(rustpython_pylib::FROZEN_STDLIB);
        });

        // Compile the expression
        let expr_str: &str = self
            .expression
            .get()
            .as_ref()
            .try_into()
            .map_err(|_| "Failed to get expression string")?;

        interp.enter(|vm| -> Result<(), &str> {
            // Create globals dictionary
            self.globals = Some(vm.ctx.new_dict().into());

            // Compile the code
            let mode = if self.file_mode.get().as_ref().try_into().unwrap_or(false) {
                vm::compiler::Mode::Exec
            } else {
                vm::compiler::Mode::Eval
            };

            let code = vm.compile(expr_str, mode, "<string>".to_owned())
                .map_err(|_| "Failed to compile Python expression")?;

            // Store the compiled code as PyObjectRef
            self.compiled_code = Some(code.into());

            // Initialize locals if not preserving state
            if !self.preserve_state.get().as_ref().try_into().unwrap_or(false) {
                self.locals = Some(vm.ctx.new_dict().into());
            }

            Ok(())
        })?;

        // Store interpreter for later use
        self.interpreter = Some(interp);
        Ok(())
    }

    fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
        self.cleanup_helper(ctx)?;
        self.interpreter = None;
        self.compiled_code = None;
        self.locals = None;
        self.globals = None;
        self.output = ClonedVar::default();
        Ok(())
    }

    fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
        self.compose_helper(data)?;
        Ok(self.output_types()[0])
    }

    fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
        let interp = self.interpreter.as_ref()
            .ok_or("Interpreter not initialized")?;

        let result = interp.enter(|vm| -> Result<Option<Var>, String> {
            // Create or reset locals dictionary
            let preserve_state: bool = self
                .preserve_state
                .get()
                .as_ref()
                .try_into()
                .map_err(|_| "Failed to get preserve_state value")?;

            if self.locals.is_none() || !preserve_state {
                self.locals = Some(vm.ctx.new_dict().into());
            }

            // Use a combined dictionary for locals and globals
            use rustpython_vm::builtins::PyDict;
            let exec_dict = if preserve_state && self.locals.is_some() {
                // Use existing locals merged with globals
                let locals = self.locals.as_ref().unwrap().clone()
                    .downcast::<PyDict>()
                    .map_err(|_| "Failed to downcast locals to dict")?;
                locals
            } else {
                // Create new dict
                vm.ctx.new_dict()
            };

            // Convert input to Python object and set as _s
            let py_input = shvar_to_py(vm, input)
                .map_err(|e| format!("Failed to convert input: {:?}", e))?;

            // Set _s variable
            exec_dict.set_item("_s", py_input, vm)
                .map_err(|e| format!("Failed to set _s variable: {:?}", e))?;

            // Execute the compiled code
            let code_obj = self
                .compiled_code
                .as_ref()
                .ok_or("No compiled code available")?
                .clone();

            use rustpython_vm::builtins::PyCode;
            let code = code_obj.downcast::<PyCode>()
                .map_err(|_| "Failed to downcast code object")?;

            // Use exec_dict as both locals and globals
            let scope = vm::scope::Scope::new(None, exec_dict.clone());
            let exec_result = vm
                .run_code_obj(code, scope)
                .map_err(|e| format!("Python execution failed: {:?}", e))?;

            // Update our locals after execution
            self.locals = Some(exec_dict.into());

            // Handle result based on mode
            let file_mode: bool = self
                .file_mode
                .get()
                .as_ref()
                .try_into()
                .map_err(|_| "Failed to get file_mode value")?;

            if file_mode {
                // In file mode, return the input unchanged
                Ok(Some(*input))
            } else {
                // In eval mode, convert and return the result
                py_to_shvar(vm, exec_result, &mut self.output)?;
                Ok(Some(self.output.0))
            }
        });

        result.map_err(|e| {
            shlog_error!("{}", e);
            "Python evaluation failed"
        })
    }
}

#[no_mangle]
pub extern "C" fn shardsRegister_py_py(core: *mut shards::shardsc::SHCore) {
    unsafe {
        shards::core::Core = core;
    }

    register_shard::<PyEvalShard>();
}
