use embassy_sync::{blocking_mutex::raw::CriticalSectionRawMutex, watch::Watch};
use static_cell::StaticCell;


pub const ROWS: usize = 4;
pub const COLS: usize = 3;


pub struct GlobalBus {
    pub btns: Watch<CriticalSectionRawMutex, [[bool; COLS]; ROWS], 2>,
}

static BUS: StaticCell<GlobalBus> = StaticCell::new();

pub fn init() -> &'static GlobalBus {
    BUS.init(GlobalBus {
        btns: Watch::new()
    })
}
