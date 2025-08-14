use rand::thread_rng;
use std::ffi::CString;
use std::os::raw::c_char;

/// Generates a random pet name with the specified number of words and separator
/// 
/// @param words_count Number of words to generate
/// @param separator Separator between words
/// @return A C string containing the generated name (must be freed by caller)
#[no_mangle]
pub extern "C" fn petname_generate(words_count: u8, separator: *const c_char) -> *mut c_char {
    let separator_str = if separator.is_null() {
        "-"
    } else {
        unsafe {
            std::ffi::CStr::from_ptr(separator)
                .to_str()
                .unwrap_or("-")
        }
    };
    
    let mut rng = thread_rng();
    let pname = petname::Petnames::default().generate(&mut rng, words_count, separator_str);
    
    let c_string = CString::new(pname).unwrap_or(CString::new("random-name").unwrap());
    c_string.into_raw()
}

/// Frees a string allocated by petname_generate
#[no_mangle]
pub extern "C" fn petname_free_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        unsafe {
            let _ = CString::from_raw(ptr);
        }
    }
}
