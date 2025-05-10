// C API
use std::ffi::{c_char, CStr, CString};
use crate::config::{ColorMode, Config, Hierarchical};
use visioncortex::{ColorImage, PathSimplifyMode};
use std::path::Path;
use crate::converter::{convert_image_to_svg, convert_image_to_svg_from_memory};

#[derive(Debug, Clone)]
#[repr(C)]
pub enum VTracerSimplifyMode {
    None,
    Polygon,
    Spline,
}

impl From<VTracerSimplifyMode> for PathSimplifyMode {
    fn from(mode: VTracerSimplifyMode) -> Self {
        match mode {
            VTracerSimplifyMode::None => PathSimplifyMode::None,
            VTracerSimplifyMode::Polygon => PathSimplifyMode::Polygon,
            VTracerSimplifyMode::Spline => PathSimplifyMode::Spline,
        }
    }
}

impl From<PathSimplifyMode> for VTracerSimplifyMode {
    fn from(mode: PathSimplifyMode) -> Self {
        match mode {
            PathSimplifyMode::None => VTracerSimplifyMode::None,
            PathSimplifyMode::Polygon => VTracerSimplifyMode::Polygon,
            PathSimplifyMode::Spline => VTracerSimplifyMode::Spline,
        }
    }
}

/// Make it C API compatible
#[derive(Debug, Clone)]
#[repr(C)]
pub struct VTracerConfig {
    pub color_mode: ColorMode,
    pub hierarchical: Hierarchical,
    pub filter_speckle: usize,
    pub color_precision: i32,
    pub layer_difference: i32,
    pub mode: VTracerSimplifyMode,
    pub corner_threshold: i32,
    pub length_threshold: f64,
    pub max_iterations: usize,
    pub splice_threshold: i32,
    pub path_precision: u32,
}

impl From<VTracerConfig> for Config {
    fn from(c_config: VTracerConfig) -> Self {
        Config {
            color_mode: c_config.color_mode.clone().into(),
            hierarchical: c_config.hierarchical.clone().into(),
            filter_speckle: c_config.filter_speckle,
            color_precision: c_config.color_precision,
            layer_difference: c_config.layer_difference,
            mode: c_config.mode.clone().into(),
            corner_threshold: c_config.corner_threshold,
            length_threshold: c_config.length_threshold,
            max_iterations: c_config.max_iterations,
            splice_threshold: c_config.splice_threshold,
            path_precision: if c_config.path_precision == u32::MAX {
                None
            } else {
                Some(c_config.path_precision)
            }
        }
    }
}


#[derive(Clone)]
#[repr(C)]
pub struct VTracerColorImage {
    /// RGBA pixels; data must be width * height * 4 bytes
    pub pixels: *mut u8,
    pub width: usize,
    pub height: usize,
}

fn u8_ptr_to_vec(ptr: *mut u8, pixels_len: usize) -> Vec<u8> {
    // Create a new vector with the required capacity
    let mut vec = Vec::with_capacity(pixels_len);
    // Set the length to match the capacity
    unsafe {
        vec.set_len(pixels_len);
    }
    // Copy the data byte by byte to avoid any potential overlap
    for i in 0..pixels_len {
        unsafe {
            vec[i] = *ptr.add(i);
        }
    }
    vec
}

impl From<VTracerColorImage> for ColorImage {
    fn from(c_image: VTracerColorImage) -> Self {
        ColorImage {
            pixels: u8_ptr_to_vec(c_image.pixels, c_image.width * c_image.height * 4),
            width: c_image.width,
            height: c_image.height
        }
    }
}



#[no_mangle]
// instead of i8, use char*
pub extern "C" fn vtracer_convert_image_to_svg(input_path: *const c_char, output_path: *const c_char, config: *const VTracerConfig) -> *const c_char {
    let input_path = unsafe { CStr::from_ptr(input_path) }.to_str().unwrap();
    let output_path = unsafe { CStr::from_ptr(output_path) }.to_str().unwrap();
    let c_config = unsafe { &*config };
    let config: Config = c_config.clone().into();


    let input_path = Path::new(input_path);
    let output_path = Path::new(output_path);

    match convert_image_to_svg(input_path, output_path, config) {
        Ok(_) => std::ptr::null(),
        Err(e) => {
            let error = CString::new(e).unwrap();
            error.into_raw()
        }
    }
}

#[no_mangle]
pub extern "C" fn vtracer_convert_image_memory_to_svg(svg_data: *const VTracerColorImage, output_path: *const c_char, config: *const VTracerConfig) -> *const c_char {
    let c_image = unsafe { &*svg_data };
    let image: ColorImage = c_image.clone().into();
    let output_path = unsafe { CStr::from_ptr(output_path) }.to_str().unwrap();
    let output_path = Path::new(output_path);
    let c_config = unsafe { &*config };
    let config: Config = c_config.clone().into();

    match convert_image_to_svg_from_memory(image, output_path, config) {
        Ok(_) => std::ptr::null(),
        Err(e) => {
            let error = CString::new(e).unwrap();
            error.into_raw()
        }
    }
}

#[no_mangle]
pub extern "C" fn vtracer_free_string(s: *const c_char) {
    if s.is_null() {
        return;
    }

    unsafe {
        let _ = CString::from_raw(s as *mut _);
    }
}
