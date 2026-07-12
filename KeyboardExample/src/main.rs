#![no_std]
#![no_main]

mod prelude;
use crate::prelude::*;

mod bus;
use bus::{ROWS, COLS};

use ch32_hal::mode::Async;
use defmt_rtt as _;
use embedded_hal::spi::SpiBus;

use core::panic::PanicInfo;
#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    // This will print the panic message, file, and line number via defmt!
    defmt::error!("{}", defmt::Display2Format(info));

    // Halt the CPU
    loop {
        core::hint::spin_loop();
    }
}


use embassy_executor::Spawner;
use embassy_futures::join::join;
use embassy_time::Timer;
use embassy_usb::class::cdc_acm::{CdcAcmClass, State};
use embassy_usb::driver::EndpointError;
use embassy_usb::Builder;
use hal::time::Hertz;
use hal::usbd::{Driver, Instance};
use hal::{bind_interrupts, peripherals};
use {ch32_hal as hal};
use hal::gpio::{Level, Input, Output, Speed, Pull};
use hal::exti::ExtiInput;
use hal::spi::Spi;
use hal::i2c::{I2c, Config as I2cConfig};

// pinmaps from edg
// [
// i2c=I2C1,
// i2c.scl=PB6, 29,
// i2c.sda=PB7, 30,
// led=PB4, 27,
// enc_a=PA3, 9,
// enc_b=PA2, 8,
// enc_sw=PA1, 7,
// oled_rst=PB1, 15,
// npx=PB5, 28,
// sw_col_0=PA8, 18,
// sw_col_1=PA6, 12,
// sw_col_2=PA4, 10,
// sw_row_0=PA10, 20,
// sw_row_1=PA9, 19,
// sw_row_2=PA7, 13,
// sw_row_3=PA5, 11,
// 0=USB,
// 0.dp=PA12, 22,
// 0.dm=PA11, 21
// ]


bind_interrupts!(struct Irqs {
    USB_LP_CAN1_RX0 => hal::usbd::InterruptHandler<hal::peripherals::USBD>;
    I2C1_EV => hal::i2c::EventInterruptHandler<hal::peripherals::I2C1>;
    I2C1_ER => hal::i2c::ErrorInterruptHandler<hal::peripherals::I2C1>;
});

// If you are trying this and your USB device doesn't connect, the most
// common issues are the RCC config and vbus_detection
//
// See https://embassy.dev/book/#_the_usb_examples_are_not_working_on_my_board_is_there_anything_else_i_need_to_configure
// for more information.
#[embassy_executor::main(entry = "qingke_rt::entry")]
async fn main(spawner: Spawner) {
    let p = hal::init(hal::Config {
        rcc: hal::rcc::Config::SYSCLK_FREQ_144MHZ_HSE,
        ..Default::default()
    });
    info!("Start");

    let bus = bus::init();

    // note, this interferes with the npx SPI and should not be used
    let _led = Output::new(p.PB4, Level::Low, Speed::Low);

    spawner.spawn(keyboard_scan_task(bus, (
        Input::new(p.PA8, Pull::Up),
        Input::new(p.PA6, Pull::Up),
        Input::new(p.PA4, Pull::Up),
    ), (
        Output::new(p.PA10, Level::High, Speed::Low),
        Output::new(p.PA9,  Level::High, Speed::Low),
        Output::new(p.PA7,  Level::High, Speed::Low),
        Output::new(p.PA5,  Level::High, Speed::Low),
    )).unwrap());

    spawner.spawn(encoder_task(bus,
        Input::new(p.PA3, Pull::Up),
        Input::new(p.PA2, Pull::Up),
    ).unwrap());

    // ExtiInput doesn't also implement Input =()
    // spawner.spawn(encoder_task(bus,
    //     ExtiInput::new(p.PA3, p.EXTI3, Pull::Up),
    //     ExtiInput::new(p.PA2, p.EXTI2, Pull::Up),
    // ).unwrap());

    spawner.spawn(encoder_sw_task(bus,
        ExtiInput::new(p.PA1, p.EXTI1, Pull::Up),
    ).unwrap());

    let mut spi_config = hal::spi::Config::default();
    spi_config.frequency = Hertz::khz(2800);
    // let spi = Spi::new_blocking_txonly(p.SPI1,p.PB3 , p.PB5, spi_config);
    let spi = Spi::new_txonly(p.SPI1,p.PB3 , p.PB5, p.DMA1_CH3, spi_config);
    spawner.spawn(npx_task(bus, spi).unwrap());

    // Initialize I2C1 with PB6 (SCL) and PB7 (SDA) using DMA for async operations
    let i2c = I2c::new(
        p.I2C1, 
        p.PB6, 
        p.PB7,
        Irqs,
        p.DMA1_CH6,
        p.DMA1_CH7,
        Hertz(400_000),
        I2cConfig::default()
    );
    let oled_rst = Output::new(p.PB1, Level::Low, Speed::Low);
    spawner.spawn(display_task(bus, i2c, oled_rst).unwrap());

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


use keyberon::matrix::Matrix;

#[embassy_executor::task]
async fn keyboard_scan_task(
    bus: &'static bus::GlobalBus,
    col_pins: (Input<'static>, Input<'static>, Input<'static>),
    row_pins: (Output<'static>, Output<'static>, Output<'static>, Output<'static>),
) {
    let btns_snd = bus.btns.sender();

    let mut matrix = Matrix::new(
        [col_pins.0, col_pins.1, col_pins.2],
        [row_pins.0, row_pins.1, row_pins.2, row_pins.3],
    ).unwrap();

    info!("matrix init'd");

    loop {
        let keys_state = matrix.get().unwrap();
        btns_snd.send(keys_state);

        Timer::after_millis(5).await; 
    }
}


use quadrature_encoder::{HalfStep, RotaryEncoder, RotaryMovement};

#[embassy_executor::task]
async fn encoder_task(
    bus: &'static bus::GlobalBus,
    a: Input<'static>,
    b: Input<'static>
) {
    let encoder_snd = bus.encoder.sender();

    let mut encoder: RotaryEncoder<_, _, HalfStep, i32, quadrature_encoder::Blocking> = RotaryEncoder::new(a, b);

    let mut counts: i32 = 0;

    info!("encoder init'd");

    loop {
        match encoder.poll().unwrap_or(None) {
            None => {},
            Some(RotaryMovement::Clockwise) => {
                counts += 1;
                encoder_snd.send(counts);
            },
            Some(RotaryMovement::CounterClockwise) => {
                counts -= 1;
                encoder_snd.send(counts);
            },
        }
        Timer::after_millis(1).await;
    }
}


#[embassy_executor::task]
async fn encoder_sw_task(
    bus: &'static bus::GlobalBus,
    mut sw: ExtiInput<'static>
) {
    let encoder_sw_snd = bus.encoder_sw.sender();

    info!("encoder sw init'd");

    loop {
        sw.wait_for_low().await;
        encoder_sw_snd.send(true);
        sw.wait_for_high().await;
        encoder_sw_snd.send(false);
    }
}


use smart_leds::SmartLedsWrite;
use ws2812_spi::prerendered::Ws2812;

#[embassy_executor::task]
async fn npx_task(bus: &'static bus::GlobalBus, spi: Spi<'static, peripherals::SPI1, Async>) {
    let mut btns_rcv = bus.btns.receiver().unwrap();

    use smart_leds::{RGB8};

    let mut npx_buf: [u8; 512] = [0; 512];
    let mut npx = Ws2812::new(spi, &mut npx_buf);

    info!("npx init'd");

    loop {
        let keys_state = btns_rcv.changed().await;
        let mut colors = [ RGB8 { r: 0, g: 0, b: 0 }; 12 ];
        for row in 0..ROWS {
            for col in 0..COLS {
                if keys_state[row][col] {
                    if row % 2 != 0 {
                        colors[row * 3 + (COLS - 1) - col] = RGB8 { r: 2, g: 0, b: 2 };
                    } else {
                        colors[row * 3 + col] = RGB8 { r: 2, g: 2, b: 0 };
                    }
                }
            }
        }

        npx.write(colors.into_iter()).unwrap();
        // TODO wait on SmartLEDs or SPI instead of guess waiting
        Timer::after_millis(10).await;
    }
}

use embedded_graphics::{
    mono_font::{ascii::FONT_5X8, MonoTextStyleBuilder},
    pixelcolor::BinaryColor,
    prelude::*,
    text::{Baseline, Text},
};
use ssd1306::{prelude::*, I2CDisplayInterface, Ssd1306Async};

use heapless::String;
use ufmt::uwrite;

#[embassy_executor::task]
async fn display_task(bus: &'static bus::GlobalBus, i2c: I2c<'static, peripherals::I2C1, Async>, mut rst: Output<'static>) {
    rst.set_low();
    Timer::after_millis(1).await;
    rst.set_high();


    let interface = I2CDisplayInterface::new(i2c);
    let mut display = Ssd1306Async::new(interface, DisplaySize128x64, DisplayRotation::Rotate0)
        .into_buffered_graphics_mode();
    display.init().await.expect("display init failed");

    let text_style = MonoTextStyleBuilder::new()
        .font(&FONT_5X8)
        .text_color(BinaryColor::On)
        .build();

    info!("display init'd");

    loop {
        let keys_state = bus.btns.try_get().unwrap_or_default();
        let encoder_count = bus.encoder.try_get().unwrap_or_default();

        let encoder_sw = bus.encoder_sw.try_get().unwrap_or_default();

        display.clear(BinaryColor::Off).unwrap();
        
        Text::with_baseline("Ducky Mechanical Keyboard", Point { x: 0, y: 0 }, text_style, Baseline::Top)
            .draw(&mut display)
            .unwrap();

        let mut s: String<32> = String::new();
        uwrite!(s, "Enc: {}", encoder_count).unwrap();
        Text::with_baseline(&s, Point { x: 0, y: 8 }, text_style, Baseline::Top)
            .draw(&mut display)
            .unwrap();

        if encoder_sw {
            Text::with_baseline("SW", Point { x: 64, y: 8 }, text_style, Baseline::Top)
                .draw(&mut display)
                .unwrap();
        }


        let matix_origin = Point { x: 64-7, y: 16 };
        for row in 0..ROWS {
            for col in 0..COLS {
                if keys_state[row][col] {
                    Text::with_baseline("X", matix_origin + Point { x: (col * 5) as i32, y: (row * 8) as i32 }, text_style, Baseline::Top)
                        .draw(&mut display)
                        .unwrap();

                }
            }
        }

        display.flush().await.inspect_err(|err| error!("display flush error: {}", defmt::Debug2Format(err))).ok();

        Timer::after_millis(33).await;
    }
}
