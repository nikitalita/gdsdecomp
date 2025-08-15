// C API
use std::ffi::{c_char, CStr, CString};
use crate::config::{ColorMode, Config, Hierarchical};
use visioncortex::{ColorImage, PathSimplifyMode};
use std::path::Path;
use crate::converter::{convert_image_to_svg, convert_image_to_svg_from_memory};

#[allow(unused_imports)]
use gifski::c_api::*;

#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub enum VTracerPathSimplifyMode {
    None = 0,
    Polygon = 1,
    Spline = 2,
}

impl Copy for ColorMode{}
impl Copy for Hierarchical{}

impl Default for VTracerPathSimplifyMode {
    fn default() -> Self {
        VTracerPathSimplifyMode::Spline
    }
}

impl From<&VTracerPathSimplifyMode> for PathSimplifyMode {
    fn from(mode: &VTracerPathSimplifyMode) -> Self {
        match mode {
            VTracerPathSimplifyMode::None => PathSimplifyMode::None,
            VTracerPathSimplifyMode::Polygon => PathSimplifyMode::Polygon,
            VTracerPathSimplifyMode::Spline => PathSimplifyMode::Spline,
        }
    }
}

impl From<VTracerPathSimplifyMode> for PathSimplifyMode {
    fn from(mode: VTracerPathSimplifyMode) -> Self {
        Self::from(&mode)
    }
}

impl From<&PathSimplifyMode> for VTracerPathSimplifyMode {
    fn from(mode: &PathSimplifyMode) -> Self {
        match mode {
            PathSimplifyMode::None => VTracerPathSimplifyMode::None,
            PathSimplifyMode::Polygon => VTracerPathSimplifyMode::Polygon,
            PathSimplifyMode::Spline => VTracerPathSimplifyMode::Spline,
        }
    }
}

impl From<PathSimplifyMode> for VTracerPathSimplifyMode {
    fn from(mode: PathSimplifyMode) -> Self {
        Self::from(&mode)
    }
}

/// VTracer Config
#[derive(Debug, Clone)]
#[repr(C)]
pub struct VTracerConfig {
    /// Color mode `color` (default) or Binary image `bw`
    pub color_mode: ColorMode,
    /// Hierarchical clustering `stacked` (default) or non-stacked `cutout`.
    /// Only applies to color mode. 
    pub hierarchical: Hierarchical,
    /// Discard patches smaller than X px in size (must be between 0 and 16, default 1)
    pub filter_speckle: usize,
    /// Number of significant bits to use in an RGB channel (must be between 1 and 8, default 6)
    pub color_precision: i32,
    /// Color difference between gradient layers (must be between 0 and 255, default 16)
    pub layer_difference: i32,
    /// Curver fitting mode `pixel`, `polygon`, `spline` (default `spline`)
    pub mode: VTracerPathSimplifyMode,
    /// Minimum momentary angle (degree) to be considered a corner (must be between 0 and 180, default 60)
    pub corner_threshold: i32,
    /// Perform iterative subdivide smooth until all segments are shorter than this length (must be between 3.5 and 10, default 4)
    pub length_threshold: f64,
    /// Maximum iterations before stopping (must be between 1 and 100, default 10)
    pub max_iterations: usize,
    /// Minimum angle displacement (degree) to splice a spline (must be between 0 and 180, default 45)
    pub splice_threshold: i32,
    /// Number of decimal places to use in path string (minimum 0, default 2)
    pub path_precision: u32,
    /// Fraction of pixels in the top/bottom rows of the image that need to be transparent before
    /// the entire image will be keyed. (between 0 and 1, default 0.2)
    pub keying_threshold: f32,
}

// implement a verify method for the config
impl VTracerConfig {
    pub fn verify(&self) -> Result<(), String> {
        let color_mode_int_value = self.color_mode as i32;
        if color_mode_int_value < ColorMode::Color as i32 || color_mode_int_value > ColorMode::Binary as i32 {
            return Err("Color mode must be Color or Binary".to_string());
        }
        let hierarchical_int_value = self.hierarchical as i32;
        if hierarchical_int_value < Hierarchical::Stacked as i32 || hierarchical_int_value > Hierarchical::Cutout as i32 {
            return Err("Hierarchical must be Stacked or Cutout".to_string());
        }
        let mode_int_value = self.mode as i32;
        if mode_int_value < VTracerPathSimplifyMode::None as i32 || mode_int_value > VTracerPathSimplifyMode::Spline as i32 {
            return Err("Mode must be None, Polygon, or Spline".to_string());
        }
        if self.filter_speckle > 16 {
            return Err("Filter speckle must be between 0 and 16".to_string());
        }
        if self.color_precision < 1 || self.color_precision > 8 {
            return Err("Color precision must be between 1 and 8".to_string());
        }
        if self.layer_difference < 0 || self.layer_difference > 255 {
            return Err("Layer difference must be between 0 and 255".to_string());
        }   
        if self.corner_threshold < 0 || self.corner_threshold > 180 {
            return Err("Corner threshold must be between 0 and 180".to_string());
        }
        if self.length_threshold < 3.5 || self.length_threshold > 10.0 {
            return Err("Length threshold must be between 3.5 and 10".to_string());
        }
        if self.max_iterations < 1 || self.max_iterations > 100 {
            return Err("Max iterations must be between 1 and 100".to_string());
        }
        if self.splice_threshold < 0 || self.splice_threshold > 180 {
            return Err("Splice threshold must be between 0 and 180".to_string());
        }
        Ok(())
    }

    pub fn to_config(&self) -> Result<Config, String> {
        if let Err(e) = self.verify() {
            return Err(e);
        }
        Ok(self.into())
    }

    pub fn set_from_config(&mut self, config: &Config) {
        self.color_mode = config.color_mode.clone().into();
        self.hierarchical = config.hierarchical.clone().into();
        self.filter_speckle = config.filter_speckle;
        self.color_precision = config.color_precision;
        self.layer_difference = config.layer_difference;
        self.mode = config.mode.clone().into();
        self.corner_threshold = config.corner_threshold;
        self.length_threshold = config.length_threshold;
        self.max_iterations = config.max_iterations;
        self.splice_threshold = config.splice_threshold;
        self.path_precision = config.path_precision.unwrap_or(u32::MAX);
        self.keying_threshold = config.keying_threshold;
    }
}


// default
impl Default for VTracerConfig {
    fn default() -> Self {
        Config::default().into()
    }
}


fn set_config_from_c_config(c_config: &VTracerConfig, config: &mut Config) {
    config.color_mode = c_config.color_mode.clone().into();
    config.hierarchical = c_config.hierarchical.clone().into();
    config.filter_speckle = c_config.filter_speckle;
    config.color_precision = c_config.color_precision;
    config.layer_difference = c_config.layer_difference;
    config.mode = c_config.mode.clone().into();
    config.corner_threshold = c_config.corner_threshold;
    config.length_threshold = c_config.length_threshold;
    config.max_iterations = c_config.max_iterations;
    config.splice_threshold = c_config.splice_threshold;
    config.path_precision = if c_config.path_precision == u32::MAX {
        None
    } else {
        Some(c_config.path_precision)
    };
    config.keying_threshold = c_config.keying_threshold;
}

impl From<VTracerConfig> for Config {
    fn from(c_config: VTracerConfig) -> Self {
        Self::from(&c_config)
    }
}

impl From<&VTracerConfig> for Config {
    fn from(c_config: &VTracerConfig) -> Self {
        let mut config: Config = Config::default();
        set_config_from_c_config(c_config, &mut config);
        config
    }
}

impl From<&Config> for VTracerConfig {
    fn from(config: &Config) -> Self {
        let mut c_config: VTracerConfig = VTracerConfig::default();
        c_config.set_from_config(config);
        c_config
    }
}

impl From<Config> for VTracerConfig {
    fn from(config: Config) -> Self {
        Self::from(&config)
    }
}



#[derive(Clone)]
#[repr(C)]
pub struct VTracerColorImage {
    /// Pointer to RGBA pixels; data must be width * height * 4 bytes
    pub pixels: *mut u8,
    /// Width of the image in pixels
    pub width: usize,
    /// Height of the image in pixels
    pub height: usize,
}

fn u8_ptr_to_vec(ptr: *mut u8, pixels_len: usize) -> Vec<u8> {
    let mut vec = Vec::with_capacity(pixels_len);
    vec.extend_from_slice(unsafe { std::slice::from_raw_parts(ptr, pixels_len) });
    vec
}


impl From<&VTracerColorImage> for ColorImage {
    fn from(c_image: &VTracerColorImage) -> Self {
        ColorImage { 
            pixels: u8_ptr_to_vec(c_image.pixels, c_image.width * c_image.height * 4),
            width: c_image.width, 
            height: c_image.height 
        }
    }
}

impl From<VTracerColorImage> for ColorImage {
    fn from(c_image: VTracerColorImage) -> Self {
        Self::from(&c_image)
    }
}


// API

/// Set the default configuration.
/// 
/// # Arguments
/// 
/// * `c_config` - The configuration to set.
#[no_mangle]
pub extern "C" fn vtracer_set_default_config(c_config: *mut VTracerConfig){
    if c_config.is_null() {
        return;
    }
    let config = Config::default();
    let c_config = unsafe { &mut *c_config };
    c_config.set_from_config(&config);
}


/// Convert an image file to SVG.
/// 
/// ## Arguments
/// 
/// * `input_path` - The path to the input image.
/// * `output_path` - The path to the output SVG file.
/// * `config` - The configuration for the conversion.
/// 
/// ## Returns
/// 
/// * `nullptr` - If the conversion is successful.
/// * `<error_message>` - If the conversion fails. If the error message is not null, you must free it using `vtracer_free_string`.
#[no_mangle]
pub extern "C" fn vtracer_convert_image_to_svg(input_path: *const c_char, output_path: *const c_char, config: *const VTracerConfig) -> *const c_char {
    let input_path = unsafe { CStr::from_ptr(input_path) }.to_str().unwrap();
    let output_path = unsafe { CStr::from_ptr(output_path) }.to_str().unwrap();
    let c_config = unsafe { &*config };

    let config = match c_config.to_config() {
        Ok(config) => config,
        Err(e) => {
            let error = CString::new(e).unwrap();
            return error.into_raw();
        }
    };

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


/// Convert an image from memory to SVG.
/// 
/// ## Arguments
/// 
/// * `svg_data` - The image data.
/// * `output_path` - The path to the output SVG file.
/// * `config` - The configuration for the conversion.
/// 
/// ## Returns
/// 
/// * `nullptr` - If the conversion is successful.
/// * `<error_message>` - If the conversion fails. If the error message is not null, you must free it using `vtracer_free_string`.
#[no_mangle]
pub extern "C" fn vtracer_convert_image_memory_to_svg(svg_data: *const VTracerColorImage, output_path: *const c_char, config: *const VTracerConfig) -> *const c_char {
    let c_image = unsafe { &*svg_data };
    let image: ColorImage = c_image.into();
    let output_path = unsafe { CStr::from_ptr(output_path) }.to_str().unwrap();
    let output_path = Path::new(output_path);
    let c_config = unsafe { &*config };

    let config = match c_config.to_config() {
        Ok(config) => config,
        Err(e) => {
            let error = CString::new(e).unwrap();
            return error.into_raw();
        }
    };

    match convert_image_to_svg_from_memory(image, output_path, config) {
        Ok(_) => std::ptr::null(),
        Err(e) => {
            let error = CString::new(e).unwrap();
            error.into_raw()
        }
    }
}


/// Free a string.
/// 
/// If the above functions returns an error message, you must call this function to free the memory; do NOT attempt to free from C code.
/// 
/// ## Arguments
/// 
/// * `s` - The string to free.
#[no_mangle]
pub extern "C" fn vtracer_free_string(s: *const c_char) {
    if s.is_null() {
        return;
    }

    unsafe {
        let _ = CString::from_raw(s as *mut _);
    }
}
