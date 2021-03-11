import ctypes, ctypes.util


gobject_path = ctypes.util.find_library('gobject-2.0') or \
               ctypes.util.find_library('libgobject-2.0-0')
if not gobject_path:
    raise ImportError('libgobject-2.0 not found')
gobject = ctypes.CDLL(gobject_path)

cairo_path = ctypes.util.find_library('cairo') or\
             ctypes.util.find_library('libcairo-2')
if not cairo_path:
    raise ImportError('libcairo not found')
cairo = ctypes.CDLL(cairo_path)

gdk_pixbuf_path = ctypes.util.find_library('gdk_pixbuf-2.0') or \
                  ctypes.util.find_library('libgdk_pixbuf-2.0-0')
if not gdk_pixbuf_path:
    raise ImportError('libgdk_pixbuf-2.0 not found')
gdk_pixbuf = ctypes.CDLL(gdk_pixbuf_path)

rsvg_path = ctypes.util.find_library('rsvg-2') or \
            ctypes.util.find_library('librsvg-2-2')
if not rsvg_path:
    raise ImportError('librsvg-2 not found')
rsvg = ctypes.CDLL(rsvg_path)


class GError(ctypes.Structure):
    _fields_ = (('domain', ctypes.c_uint32),
                ('code', ctypes.c_int),
                ('message', ctypes.c_char_p))
GErrorP = ctypes.POINTER(GError)
GErrorPP = ctypes.POINTER(GErrorP)


gobject.g_type_init()

gobject.g_object_unref.argtypes = [ctypes.c_void_p]


GdkPixbufP = ctypes.c_void_p
GdkPixbufSaveFunc = ctypes.CFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_ulong, GErrorPP, ctypes.c_void_p)

gdk_pixbuf.gdk_pixbuf_save_to_callbackv.argtypes = [GdkPixbufP, GdkPixbufSaveFunc, ctypes.c_void_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p), ctypes.POINTER(ctypes.c_char_p), GErrorPP]
gdk_pixbuf.gdk_pixbuf_save_to_callbackv.restype = ctypes.c_bool


cairo_tp = ctypes.c_void_p
cairo_surface_tp = ctypes.c_void_p
cairo_write_func_t = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint)

class cairo_matrix_t(ctypes.Structure):
    _fields_ = (('xx', ctypes.c_double),
                ('yx', ctypes.c_double),
                ('xy', ctypes.c_double),
                ('yy', ctypes.c_double),
                ('x0', ctypes.c_double),
                ('y0', ctypes.c_double))
cairo_matrix_tp = ctypes.POINTER(cairo_matrix_t)

CAIRO_STATUS_SUCCESS   = 0
CAIRO_STATUS_NO_MEMORY = 1

CAIRO_FORMAT_INVALID   = -1
CAIRO_FORMAT_ARGB32    = 0
CAIRO_FORMAT_RGB24     = 1
CAIRO_FORMAT_A8        = 2
CAIRO_FORMAT_A1        = 3
CAIRO_FORMAT_RGB16_565 = 4
CAIRO_FORMAT_RGB30     = 5

cairo.cairo_create.argtypes = [cairo_surface_tp]
cairo.cairo_create.restype = cairo_tp

cairo.cairo_destroy.argtypes = [cairo_tp]

cairo.cairo_status.argtypes = [cairo_tp]

cairo.cairo_set_source_rgba.argtypes = [cairo_tp, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]

cairo.cairo_paint.argtypes = [cairo_tp]

cairo.cairo_scale.argtypes = [cairo_tp, ctypes.c_double, ctypes.c_double]

cairo.cairo_set_matrix.argtypes = [cairo_tp, cairo_matrix_tp]

cairo.cairo_image_surface_create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
cairo.cairo_image_surface_create.restype = cairo_surface_tp

cairo.cairo_surface_destroy.argtypes = [cairo_surface_tp]

cairo.cairo_surface_status.argtypes = [cairo_surface_tp]

cairo.cairo_surface_write_to_png_stream.argtypes = [cairo_surface_tp, cairo_write_func_t, ctypes.c_void_p]
cairo.cairo_surface_write_to_png_stream.restype = ctypes.c_int


RsvgHandleP = ctypes.c_void_p

class RsvgDimensionData(ctypes.Structure):
    _fields_ = (('width', ctypes.c_int),
                ('height', ctypes.c_int),
                ('em', ctypes.c_double),
                ('ex', ctypes.c_double))

rsvg.rsvg_handle_new.restype = RsvgHandleP

rsvg.rsvg_handle_write.argtypes = [RsvgHandleP, ctypes.c_void_p, ctypes.c_ulong, GErrorPP]
rsvg.rsvg_handle_write.restype = ctypes.c_bool

rsvg.rsvg_handle_close.argtypes = [RsvgHandleP, GErrorPP]

rsvg.rsvg_handle_get_dimensions.argtypes = [RsvgHandleP, ctypes.POINTER(RsvgDimensionData)]

rsvg.rsvg_handle_get_pixbuf.argtypes = [RsvgHandleP]
rsvg.rsvg_handle_get_pixbuf.restype = GdkPixbufP

rsvg.rsvg_handle_render_cairo.argtypes = [RsvgHandleP, cairo_tp]
rsvg.rsvg_handle_render_cairo.restype = ctypes.c_bool
