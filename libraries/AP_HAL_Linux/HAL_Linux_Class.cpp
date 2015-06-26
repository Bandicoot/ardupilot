#include <AP_HAL.h>
#if CONFIG_HAL_BOARD == HAL_BOARD_LINUX

#include "HAL_Linux_Class.h"
#include "AP_HAL_Linux_Private.h"

#include <getopt.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <AP_HAL_Empty.h>
#include <AP_HAL_Empty_Private.h>

using namespace Linux;

// 3 serial ports on Linux for now
static LinuxUARTDriver uartADriver(true);
#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_NAVIO
static LinuxSPIUARTDriver uartBDriver;
#else
static LinuxUARTDriver uartBDriver(false);
#endif
static LinuxUARTDriver uartCDriver(false);
static LinuxUARTDriver uartEDriver(false);

#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_BEBOP
static LinuxSemaphore  i2cSemaphore0;
static LinuxI2CDriver  i2cDriver0(&i2cSemaphore0, "/dev/i2c-0");
static LinuxSemaphore  i2cSemaphore1;
static LinuxI2CDriver  i2cDriver1(&i2cSemaphore1, "/dev/i2c-1");
static LinuxSemaphore  i2cSemaphore2;
static LinuxI2CDriver  i2cDriver2(&i2cSemaphore2, "/dev/i2c-2");
#else
static LinuxSemaphore  i2cSemaphore0;
static LinuxI2CDriver  i2cDriver0(&i2cSemaphore0, "/dev/i2c-1");
#endif
static LinuxSPIDeviceManager spiDeviceManager;
#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_NAVIO
static NavioAnalogIn analogIn;
#else
static LinuxAnalogIn analogIn;
#endif

/*
  select between FRAM and FS
 */
#if LINUX_STORAGE_USE_FRAM == 1
static LinuxStorage_FRAM storageDriver;
#else
static LinuxStorage storageDriver;
#endif

/*
  use the BBB gpio driver on ERLE, PXF and BBBMINI
 */
#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_PXF || CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_ERLE || CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_BBBMINI
static LinuxGPIO_BBB gpioDriver;
/*
  use the RPI gpio driver on Navio
 */
#elif CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_NAVIO
static LinuxGPIO_RPI gpioDriver;
#else
static Empty::EmptyGPIO gpioDriver;
#endif

/*
  use the PRU based RCInput driver on ERLE and PXF
 */
#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_PXF || CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_ERLE
static LinuxRCInput_PRU rcinDriver;
#elif CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_BBBMINI
static LinuxRCInput_AioPRU rcinDriver;
#elif CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_NAVIO
static LinuxRCInput_Navio rcinDriver;
#elif CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_ZYNQ
static LinuxRCInput_ZYNQ rcinDriver;
#else
static LinuxRCInput rcinDriver;
#endif

/*
  use the PRU based RCOutput driver on ERLE and PXF
 */
#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_PXF || CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_ERLE
static LinuxRCOutput_PRU rcoutDriver;
#elif CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_BBBMINI
static LinuxRCOutput_AioPRU rcoutDriver;
/*
  use the PCA9685 based RCOutput driver on Navio
 */
#elif CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_NAVIO
static LinuxRCOutput_Navio rcoutDriver;
#elif CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_ZYNQ
static LinuxRCOutput_ZYNQ rcoutDriver;
#elif CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_BEBOP
static LinuxRCOutput_Bebop rcoutDriver;
#else
static Empty::EmptyRCOutput rcoutDriver;
#endif

static LinuxScheduler schedulerInstance;
static LinuxUtil utilInstance;

HAL_Linux::HAL_Linux() :
    AP_HAL::HAL(
        &uartADriver,
        &uartBDriver,
        &uartCDriver,
        NULL,            /* no uartD */
        &uartEDriver,
#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_BEBOP
        &i2cDriver0,
        &i2cDriver1,
        &i2cDriver2,
#else
        &i2cDriver0,
        NULL,
        NULL,
#endif
        &spiDeviceManager,
        &analogIn,
        &storageDriver,
        &uartADriver,
        &gpioDriver,
        &rcinDriver,
        &rcoutDriver,
        &schedulerInstance,
        &utilInstance)
{}

void _usage(void)
{
    printf("Usage: -A uartAPath -B uartBPath -C uartCPath\n");
    printf("Options:\n");
    printf("\t-serial:          -A /dev/ttyO4\n");
    printf("\t                  -B /dev/ttyS1\n");    
    printf("\t-tcp:             -C tcp:192.168.2.15:1243:wait\n");
    printf("\t                  -A tcp:11.0.0.2:5678\n");    
    printf("\t                  -A udp:11.0.0.2:5678\n");    
}

void HAL_Linux::init(int argc,char* const argv[]) const 
{
    int opt;
    /*
      parse command line options
     */
    while ((opt = getopt(argc, argv, "A:B:C:E:h")) != -1) {
        switch (opt) {
        case 'A':
            uartADriver.set_device_path(optarg);
            break;
        case 'B':
            uartBDriver.set_device_path(optarg);
            break;
        case 'C':
            uartCDriver.set_device_path(optarg);
            break;
        case 'E':
            uartEDriver.set_device_path(optarg);
            break;
        case 'h':
            _usage();
            exit(0);
        default:
            printf("Unknown option '%c'\n", (char)opt);
            exit(1);
        }
    }

    scheduler->init(NULL);
    gpio->init();
#if CONFIG_HAL_BOARD_SUBTYPE == HAL_BOARD_SUBTYPE_LINUX_BEBOP
    i2c0->begin();
    i2c1->begin();
    i2c2->begin();
#else
    i2c0->begin();
#endif
    rcout->init(NULL);
    rcin->init(NULL);
    uartA->begin(115200);    
    uartE->begin(115200);    
    spi->init(NULL);
    analogin->init(NULL);
    utilInstance.init(argc, argv);
}

const HAL_Linux AP_HAL_Linux;

#endif
