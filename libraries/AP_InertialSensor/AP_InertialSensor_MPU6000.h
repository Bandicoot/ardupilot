/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

#ifndef __AP_INERTIAL_SENSOR_MPU6000_H__
#define __AP_INERTIAL_SENSOR_MPU6000_H__

#include <stdint.h>
#include <AP_HAL.h>
#include <AP_Math.h>
#include <AP_Progmem.h>
#include "AP_InertialSensor.h"

// enable debug to see a register dump on startup
#define MPU6000_DEBUG 0

// on fast CPUs we sample at 1kHz and use a software filter
#if HAL_CPU_CLASS >= HAL_CPU_CLASS_75
#define MPU6000_FAST_SAMPLING 1
#else
#define MPU6000_FAST_SAMPLING 0
#endif

#if MPU6000_FAST_SAMPLING
#include <Filter.h>
#include <LowPassFilter2p.h>
#endif

#define MAX_DATA_READ 1024
/* one resister address followed by seven 2-byte registers */
struct PACKED data_frame {
    uint8_t cmd;
    uint8_t int_status;
    uint8_t d[MAX_DATA_READ];
};

class AP_MPU6000_BusDriver
{
    public:
        virtual void init() = 0;
        virtual void read8(uint8_t reg, uint8_t *val) = 0;
        virtual void write8(uint8_t reg, uint8_t val) = 0;
        enum bus_speed {
                SPEED_LOW, SPEED_HIGH
        };
        virtual void set_bus_speed(AP_HAL::SPIDeviceDriver::bus_speed speed) = 0;
        virtual void read_burst(struct data_frame *rx,
                                AP_HAL::DigitalSource *_drdy_pin,
                                uint8_t *n_samples, uint8_t *sample_size) = 0;
        virtual AP_HAL::Semaphore* get_semaphore() = 0;
};

class AP_InertialSensor_MPU6000 : public AP_InertialSensor_Backend
{
public:
    AP_InertialSensor_MPU6000(AP_InertialSensor &imu, AP_MPU6000_BusDriver *bus);

    /* update accel and gyro state */
    bool update();

    bool gyro_sample_available(void) { return _sum_count >= _sample_count; }
    bool accel_sample_available(void) { return _sum_count >= _sample_count; }

private:
#if MPU6000_DEBUG
    void _dump_registers(void);
#endif

    // instance numbers of accel and gyro data
    uint8_t _gyro_instance;
    uint8_t _accel_instance;

    AP_HAL::DigitalSource *_drdy_pin;

    bool                 _init_sensor(void);
    bool                 _sample_available();
    void                 _read_data_transaction();
    bool                 _data_ready();
    void                 _poll_data(void);
    uint8_t              _register_read( uint8_t reg);
    void                 _register_write( uint8_t reg, uint8_t val );
    void                 _register_write_check(uint8_t reg, uint8_t val);
    bool                 _hardware_init(void);
    void                 _accumulate(struct data_frame *rx, uint8_t n_samples, uint8_t sample_size);

    AP_MPU6000_BusDriver *_bus;
    AP_HAL::Semaphore    *_bus_sem;

    static const float          _gyro_scale;

    // support for updating filter at runtime
    int8_t _last_accel_filter_hz;
    int8_t _last_gyro_filter_hz;

    void _set_filter_register(uint16_t filter_hz);

    // how many hardware samples before we report a sample to the caller
    uint8_t _sample_count;

#if MPU6000_FAST_SAMPLING
    Vector3f _accel_filtered;
    Vector3f _gyro_filtered;

    // Low Pass filters for gyro and accel 
    LowPassFilter2pVector3f _accel_filter;
    LowPassFilter2pVector3f _gyro_filter;
#else
    // accumulation in timer - must be read with timer disabled
    // the sum of the values since last read
    Vector3l _accel_sum;
    Vector3l _gyro_sum;
#endif
    volatile uint16_t _sum_count;
};

class AP_MPU6000_BusDriver_SPI : public AP_MPU6000_BusDriver
{
    public:
        void init();
        void read8(uint8_t reg, uint8_t *val);
        void write8(uint8_t reg, uint8_t val);
        void set_bus_speed(AP_HAL::SPIDeviceDriver::bus_speed speed);
        void read_burst(struct data_frame *rx,
                        AP_HAL::DigitalSource *_drdy_pin,
                        uint8_t *n_samples, uint8_t *sample_size);
        AP_HAL::Semaphore* get_semaphore();

    private:
        AP_HAL::SPIDeviceDriver *_spi;
        AP_HAL::Semaphore *_spi_sem;
        // count of bus errors
        uint16_t _error_count;
};

class AP_MPU6000_BusDriver_I2C : public AP_MPU6000_BusDriver
{
    public:
        AP_MPU6000_BusDriver_I2C(AP_HAL::I2CDriver *i2c, uint8_t addr);
        void init();
        void read8(uint8_t reg, uint8_t *val);
        void write8(uint8_t reg, uint8_t val);
        void set_bus_speed(AP_HAL::SPIDeviceDriver::bus_speed speed);
        void read_burst(struct data_frame *rx,
                        AP_HAL::DigitalSource *_drdy_pin,
                        uint8_t *n_samples, uint8_t *sample_size);
        AP_HAL::Semaphore* get_semaphore();

    private:
        AP_HAL::I2CDriver *_i2c;
        uint8_t _addr;
        AP_HAL::Semaphore *_i2c_sem;
};

#endif // __AP_INERTIAL_SENSOR_MPU6000_H__
