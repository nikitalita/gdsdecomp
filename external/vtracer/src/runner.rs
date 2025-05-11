use visioncortex::{Color, ColorImage, Shape};
use visioncortex::color_clusters::{Builder, BuilderImpl, Clusters,IncrementalBuilder, Cluster, NeighbourInfo, KeyingAction, HIERARCHICAL_MAX};

pub struct Runner {
    config: RunnerConfig,
    image: ColorImage,
}

pub struct RunnerConfig {
    pub diagonal: bool,
    pub hierarchical: u32,
    pub batch_size: i32,
    pub good_min_area: usize,
    pub good_max_area: usize,
    pub is_same_color_a: i32,
    pub is_same_color_b: i32,
    pub deepen_diff: i32,
    pub hollow_neighbours: usize,
    pub key_color: Color,
    pub keying_action: KeyingAction,
}

impl Default for RunnerConfig {
    fn default() -> Self {
        Self {
            diagonal: false,
            hierarchical: HIERARCHICAL_MAX,
            batch_size: 25600,
            good_min_area: 16,
            good_max_area: 256 * 256,
            is_same_color_a: 4,
            is_same_color_b: 1,
            deepen_diff: 64,
            hollow_neighbours: 1,
            key_color: Color::default(),
            keying_action: KeyingAction::default(),
        }
    }
}

impl Default for Runner {
    fn default() -> Self {
        Self {
            config: RunnerConfig::default(),
            image: ColorImage::new(),
        }
    }
}

impl Runner {

    pub fn new(config: RunnerConfig, image: ColorImage) -> Self {
        Self {
            config,
            image
        }
    }

    pub fn init(&mut self, image: ColorImage) {
        self.image = image;
    }

    pub fn builder(self) -> Builder {
        let RunnerConfig {
            diagonal,
            hierarchical,
            batch_size,
            good_min_area,
            good_max_area,
            is_same_color_a,
            is_same_color_b,
            deepen_diff,
            hollow_neighbours,
            key_color,
            keying_action,
        } = self.config;

        assert!(is_same_color_a < 8);
        let width = self.image.width;

        Builder::new()
            .from(self.image)
            .diagonal(diagonal)
            .hierarchical(hierarchical)
            .key(key_color)
            .keying_action(keying_action)
            .batch_size(batch_size as u32)
            .same(move |a: Color, b: Color| {
                color_same(a, b, is_same_color_a, is_same_color_b)
            })
            .diff(color_diff)
            .deepen(move |_internal: &BuilderImpl, patch: &Cluster, neighbours: &[NeighbourInfo]| {
                patch_good(width, patch, good_min_area, good_max_area) &&
                neighbours[0].diff > deepen_diff
            })
            .hollow(move |_internal: &BuilderImpl, _patch: &Cluster, neighbours: &[NeighbourInfo]| {
                neighbours.len() <= hollow_neighbours
            })
    }

    pub fn start(self) -> IncrementalBuilder {
        self.builder().start()
    }

    pub fn run(self) -> Clusters {
        self.builder().run()
    }

}

pub struct ColorI32Alpha{
    r: i32,
    g: i32,
    b: i32,
    a: i32,
}

impl ColorI32Alpha {
    fn new(color: &Color) -> Self {
        Self { r: color.r as i32, g: color.g as i32, b: color.b as i32, a: color.a as i32 }
    }
    pub fn add(&self, other: &Self) -> Self {
        Self {
            r: self.r + other.r,
            g: self.g + other.g,
            b: self.b + other.b,
            a: self.a + other.a,
        }
    }

    fn diff(&self, other: &Self) -> Self {
        Self {
            r: self.r - other.r,
            g: self.g - other.g,
            b: self.b - other.b,
            a: self.a - other.a,
        }
    }
    pub fn to_color(&self) -> Color {
        assert!(0 <= self.r && self.r < 256);
        assert!(0 <= self.g && self.g < 256);
        assert!(0 <= self.b && self.b < 256);
        assert!(0 <= self.a && self.a < 256);
        Color::new_rgba(self.r as u8, self.g as u8, self.b as u8, self.a as u8)
    }

}

pub fn color_diff(a: Color, b: Color) -> i32 {
    let a = ColorI32Alpha::new(&a);
    let b = ColorI32Alpha::new(&b);
    (a.r - b.r).abs() + (a.g - b.g).abs() + (a.b - b.b).abs() + (a.a - b.a).abs()
}

pub fn color_same(a: Color, b: Color, shift: i32, thres: i32) -> bool {
    let diff = ColorI32Alpha {
        r: (a.r >> shift) as i32,
        g: (a.g >> shift) as i32,
        b: (a.b >> shift) as i32,
        a: (a.a >> shift) as i32,
    }
    .diff(&ColorI32Alpha {
        r: (b.r >> shift) as i32,
        g: (b.g >> shift) as i32,
        b: (b.b >> shift) as i32,
        a: (b.a >> shift) as i32,
    });

    diff.r.abs() <= thres && diff.g.abs() <= thres && diff.b.abs() <= thres && diff.a.abs() <= thres
}

fn patch_good(
    width: usize,
    patch: &Cluster,
    good_min_area: usize,
    good_max_area: usize
) -> bool {
    if good_min_area < patch.area() && patch.area() < good_max_area {
        if good_min_area == 0 ||
            (Shape::image_boundary_list(&patch.to_image_with_hole(width as u32, true)).len() as usize) < patch.area() {
            return true;
        } else {
            // cluster is thread-like and thinner than 2px
        }
    }
    false
}
