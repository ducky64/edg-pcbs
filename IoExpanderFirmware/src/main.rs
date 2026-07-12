#![no_std]
#![no_main]


use {ch32_hal as hal};

#[panic_handler]
fn panic(info: &core::panic::PanicInfo) -> ! {
    let _ = hal::println!("\n\n\n{}", info);

    loop {}
}


use hal::Peri;
use embassy_executor::Spawner;
use embassy_time::Timer;
use hal::gpio::{AnyPin, Level, Output};
use hal::println;

/// pinning from edg [
/// pwr_sense=PC4, 14, 
/// eth_grn=PD0, 8, 
/// eth_yel=PC0, 10, 
/// i2c=I2C_T, 
/// i2c.scl=PC2, 12, 
/// i2c.sda=PC1, 11
/// ]


#[embassy_executor::task(pool_size = 2)]
async fn blink(pin: Peri<'static, AnyPin>, interval_ms: u64) {
    let mut led = Output::new(pin, Level::Low, Default::default());

    loop {
        led.set_high();
        Timer::after_millis(interval_ms).await;
        led.set_low();
        Timer::after_millis(interval_ms).await;
    }
}

#[embassy_executor::main(entry = "qingke_rt::entry")]
async fn main(spawner: Spawner) -> ! {
    hal::debug::SDIPrint::enable();
    let mut config = hal::Config::default();
    config.rcc = hal::rcc::Config::SYSCLK_FREQ_48MHZ_HSI;
    let p = hal::init(config);

    println!("CHIP signature => {}", hal::signature::chip_id().name());
    println!("Clocks {:?}", hal::rcc::clocks());

    // let mut led = Output::new(p.PC4, Level::Low, Default::default());

    spawner.spawn(blink(p.PD0.into(), 110).unwrap());
    spawner.spawn(blink(p.PC0.into(), 270).unwrap());

    loop {
        Timer::after_millis(1000).await;
        println!("tick");
    }
}



// use hal::delay::Delay;
// use hal::gpio::{Level, Output};

// #[qingke_rt::entry]
// fn main() -> ! {
//     hal::debug::SDIPrint::enable();
//     let mut config = hal::Config::default();
//     config.rcc = hal::rcc::Config::SYSCLK_FREQ_48MHZ_HSI;
//     let p = hal::init(config);

//     let mut delay = Delay;

//     let mut led1 = Output::new(p.PC0, Level::Low, Default::default());
//     led1.toggle();
//     let mut led2 = Output::new(p.PD0, Level::Low, Default::default());
//     loop {
//         led1.toggle();
//         led2.toggle();

//         delay.delay_ms(1000);
//         hal::println!("toggle!");
//         let val = hal::pac::SYSTICK.cnt().read();
//         hal::println!("systick: {}", val);
//     }
// }
