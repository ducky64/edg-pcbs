#![no_std]
#![no_main]

mod prelude;
use crate::prelude::*;

use ch32_hal::mode::{Async, Blocking};
use defmt_rtt as _;

use embassy_executor::Spawner;
use embassy_futures::join::join;
use embassy_time::Timer;
use embassy_usb::class::cdc_acm::{CdcAcmClass, State};
use embassy_usb::driver::EndpointError;
use embassy_usb::Builder;
use hal::time::Hertz;
use hal::usbd::{Driver, Instance};
use hal::{bind_interrupts, peripherals, println, usb, Config};
use {ch32_hal as hal, panic_halt as _};
use hal::gpio::{Level, Output, Speed};
use hal::spi::Spi;

use smart_leds::SmartLedsWrite;
use ws2812_spi::prerendered::Ws2812;

bind_interrupts!(struct Irqs {
    USB_LP_CAN1_RX0 => hal::usbd::InterruptHandler<hal::peripherals::USBD>;
});

// If you are trying this and your USB device doesn't connect, the most
// common issues are the RCC config and vbus_detection
//
// See https://embassy.dev/book/#_the_usb_examples_are_not_working_on_my_board_is_there_anything_else_i_need_to_configure
// for more information.
#[embassy_executor::main(entry = "qingke_rt::entry")]
async fn main(spawner: Spawner) {
    let p = hal::init(hal::Config {
        rcc: hal::rcc::Config::SYSCLK_FREQ_144MHZ_HSI,
        ..Default::default()
    });
    info!("Start");

    // note, this interferes with the npx SPI and should not be used
    let _led = Output::new(p.PB4, Level::Low, Speed::Low);

    let mut spi_config = hal::spi::Config::default();
    spi_config.frequency = Hertz::khz(3000);
    // let spi = Spi::new_blocking_txonly(p.SPI1,p.PB3 , p.PB5, spi_config);
    let spi = Spi::new_txonly(p.SPI1,p.PB3 , p.PB5, p.DMA1_CH3, spi_config);
    spawner.spawn(npx_task(spi).expect("npx task"));

    let driver = Driver::new(p.USBD, Irqs, p.PA12, p.PA11);

    // Create embassy-usb Config
    let mut config = embassy_usb::Config::new(0xC0DE, 0xCAFE);
    config.manufacturer = Some("Embassy");
    config.product = Some("USB-serial example");
    config.serial_number = Some("12345678");
    config.max_power = 100;
    config.max_packet_size_0 = 64;

    // Windows compatibility requires these; CDC-ACM
    config.device_class = 0x02;
    config.device_sub_class = 0x02;
    config.device_protocol = 0x00;
    config.composite_with_iads = false;

    // Create embassy-usb DeviceBuilder using the driver and config.
    // It needs some buffers for building the descriptors.
    let mut config_descriptor = [0; 256];
    let mut bos_descriptor = [0; 256];
    let mut msos_descriptor = [0; 256];
    let mut control_buf = [0; 64];

    let mut state = State::new();

    let mut builder = Builder::new(
        driver,
        config,
        &mut config_descriptor,
        &mut bos_descriptor,
        &mut [], // no msos descriptors
        &mut control_buf,
    );

    // Create classes on the builder.
    let mut class = CdcAcmClass::new(&mut builder, &mut state, 64);

    // Build the builder.
    let mut usb = builder.build();

    // Run the USB device.
    let usb_fut = usb.run();

    // Do stuff with the class!
    let echo_fut = async {
        loop {
            class.wait_connection().await;
            let _ = echo(&mut class).await;
        }
    };

    info!("USB init");

    // Run everything concurrently.
    // If we had made everything `'static` above instead, we could do this using separate tasks instead.
    join(usb_fut, echo_fut).await;
}

struct Disconnected {}

impl From<EndpointError> for Disconnected {
    fn from(val: EndpointError) -> Self {
        match val {
            EndpointError::BufferOverflow => panic!("Buffer overflow"),
            EndpointError::Disabled => Disconnected {},
        }
    }
}

async fn echo<'d, T: Instance + 'd>(class: &mut CdcAcmClass<'d, Driver<'d, T>>) -> Result<(), Disconnected> {
    let mut buf = [0; 64];
    loop {
        let n = class.read_packet(&mut buf).await?;
        let data = &buf[..n];
        class.write_packet(data).await?;
    }
}

#[embassy_executor::task]
async fn npx_task(mut spi: Spi<'static, peripherals::SPI1, Async>) {
    use smart_leds::{RGB8};

    let mut colors = [
        RGB8 { r: 32, g: 32, b: 0 },
        RGB8 { r: 16, g: 0, b: 16 },
        RGB8 { r: 0, g: 32, b: 32 }
    ];

    let mut colors2 = [
        RGB8 { r: 32, g: 0, b: 32 },
        RGB8 { r: 16, g: 0, b: 16 },
        RGB8 { r: 0, g: 32, b: 32 }
    ];

    let mut npx_buf: [u8; 512] = [0; 512];
    let mut npx = Ws2812::new(spi, &mut npx_buf);

    info!("NPX task start");

    loop {
        npx.write(colors.into_iter()).unwrap();
        Timer::after_millis(100).await;
        npx.write(colors2.into_iter()).unwrap();
        Timer::after_millis(100).await;
    }
}
