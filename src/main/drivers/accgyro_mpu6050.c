/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "build_config.h"
#include "debug.h"

#include "common/maths.h"

#include "drivers/pin_debug.h"
#include "drivers/exti.h"

#include "sensors/acceleration.h"
#include "sensors/gyro.h"

#include "nvic.h"

#include "system.h"
#include "gpio.h"
#include "bus_i2c.h"

#include "sensor.h"
#include "accgyro.h"
#include "accgyro_mpu6050.h"

//#define DEBUG_MPU_DATA_READY_INTERRUPT

// MPU6050, Standard address 0x68
// MPU_INT on PB13 on rev4 Naze32 hardware
#define MPU6050_ADDRESS         0x68

#define DMP_MEM_START_ADDR 0x6E
#define DMP_MEM_R_W 0x6F

// RA = Register Address

#define MPU_RA_XG_OFFS_TC       0x00    //[7] PWR_MODE, [6:1] XG_OFFS_TC, [0] OTP_BNK_VLD
#define MPU_RA_YG_OFFS_TC       0x01    //[7] PWR_MODE, [6:1] YG_OFFS_TC, [0] OTP_BNK_VLD
#define MPU_RA_ZG_OFFS_TC       0x02    //[7] PWR_MODE, [6:1] ZG_OFFS_TC, [0] OTP_BNK_VLD
#define MPU_RA_X_FINE_GAIN      0x03    //[7:0] X_FINE_GAIN
#define MPU_RA_Y_FINE_GAIN      0x04    //[7:0] Y_FINE_GAIN
#define MPU_RA_Z_FINE_GAIN      0x05    //[7:0] Z_FINE_GAIN
#define MPU_RA_XA_OFFS_H        0x06    //[15:0] XA_OFFS
#define MPU_RA_XA_OFFS_L_TC     0x07
#define MPU_RA_YA_OFFS_H        0x08    //[15:0] YA_OFFS
#define MPU_RA_YA_OFFS_L_TC     0x09
#define MPU_RA_ZA_OFFS_H        0x0A    //[15:0] ZA_OFFS
#define MPU_RA_ZA_OFFS_L_TC     0x0B
#define MPU_RA_PRODUCT_ID       0x0C    // Product ID Register
#define MPU_RA_XG_OFFS_USRH     0x13    //[15:0] XG_OFFS_USR
#define MPU_RA_XG_OFFS_USRL     0x14
#define MPU_RA_YG_OFFS_USRH     0x15    //[15:0] YG_OFFS_USR
#define MPU_RA_YG_OFFS_USRL     0x16
#define MPU_RA_ZG_OFFS_USRH     0x17    //[15:0] ZG_OFFS_USR
#define MPU_RA_ZG_OFFS_USRL     0x18
#define MPU_RA_SMPLRT_DIV       0x19
#define MPU_RA_CONFIG           0x1A
#define MPU_RA_GYRO_CONFIG      0x1B
#define MPU_RA_ACCEL_CONFIG     0x1C
#define MPU_RA_FF_THR           0x1D
#define MPU_RA_FF_DUR           0x1E
#define MPU_RA_MOT_THR          0x1F
#define MPU_RA_MOT_DUR          0x20
#define MPU_RA_ZRMOT_THR        0x21
#define MPU_RA_ZRMOT_DUR        0x22
#define MPU_RA_FIFO_EN          0x23
#define MPU_RA_I2C_MST_CTRL     0x24
#define MPU_RA_I2C_SLV0_ADDR    0x25
#define MPU_RA_I2C_SLV0_REG     0x26
#define MPU_RA_I2C_SLV0_CTRL    0x27
#define MPU_RA_I2C_SLV1_ADDR    0x28
#define MPU_RA_I2C_SLV1_REG     0x29
#define MPU_RA_I2C_SLV1_CTRL    0x2A
#define MPU_RA_I2C_SLV2_ADDR    0x2B
#define MPU_RA_I2C_SLV2_REG     0x2C
#define MPU_RA_I2C_SLV2_CTRL    0x2D
#define MPU_RA_I2C_SLV3_ADDR    0x2E
#define MPU_RA_I2C_SLV3_REG     0x2F
#define MPU_RA_I2C_SLV3_CTRL    0x30
#define MPU_RA_I2C_SLV4_ADDR    0x31
#define MPU_RA_I2C_SLV4_REG     0x32
#define MPU_RA_I2C_SLV4_DO      0x33
#define MPU_RA_I2C_SLV4_CTRL    0x34
#define MPU_RA_I2C_SLV4_DI      0x35
#define MPU_RA_I2C_MST_STATUS   0x36
#define MPU_RA_INT_PIN_CFG      0x37
#define MPU_RA_INT_ENABLE       0x38
#define MPU_RA_DMP_INT_STATUS   0x39
#define MPU_RA_INT_STATUS       0x3A
#define MPU_RA_ACCEL_XOUT_H     0x3B
#define MPU_RA_ACCEL_XOUT_L     0x3C
#define MPU_RA_ACCEL_YOUT_H     0x3D
#define MPU_RA_ACCEL_YOUT_L     0x3E
#define MPU_RA_ACCEL_ZOUT_H     0x3F
#define MPU_RA_ACCEL_ZOUT_L     0x40
#define MPU_RA_TEMP_OUT_H       0x41
#define MPU_RA_TEMP_OUT_L       0x42
#define MPU_RA_GYRO_XOUT_H      0x43
#define MPU_RA_GYRO_XOUT_L      0x44
#define MPU_RA_GYRO_YOUT_H      0x45
#define MPU_RA_GYRO_YOUT_L      0x46
#define MPU_RA_GYRO_ZOUT_H      0x47
#define MPU_RA_GYRO_ZOUT_L      0x48
#define MPU_RA_EXT_SENS_DATA_00 0x49
#define MPU_RA_MOT_DETECT_STATUS    0x61
#define MPU_RA_I2C_SLV0_DO      0x63
#define MPU_RA_I2C_SLV1_DO      0x64
#define MPU_RA_I2C_SLV2_DO      0x65
#define MPU_RA_I2C_SLV3_DO      0x66
#define MPU_RA_I2C_MST_DELAY_CTRL   0x67
#define MPU_RA_SIGNAL_PATH_RESET    0x68
#define MPU_RA_MOT_DETECT_CTRL      0x69
#define MPU_RA_USER_CTRL        0x6A
#define MPU_RA_PWR_MGMT_1       0x6B
#define MPU_RA_PWR_MGMT_2       0x6C
#define MPU_RA_BANK_SEL         0x6D
#define MPU_RA_MEM_START_ADDR   0x6E
#define MPU_RA_MEM_R_W          0x6F
#define MPU_RA_DMP_CFG_1        0x70
#define MPU_RA_DMP_CFG_2        0x71
#define MPU_RA_FIFO_COUNTH      0x72
#define MPU_RA_FIFO_COUNTL      0x73
#define MPU_RA_FIFO_R_W         0x74
#define MPU_RA_WHO_AM_I         0x75

// RF = Register Flag
// MPU_RA_FIFO_EN
#define MPU_RF_TEMP_FIFO_EN     0x80
#define MPU_RF_XG_FIFO_EN       0x40
#define MPU_RF_YG_FIFO_EN       0x20
#define MPU_RF_ZG_FIFO_EN       0x10
#define MPU_RF_ACCEL_FIFO_EN    0x08
#define MPU_RF_SLV2_FIFO_EN     0x04
#define MPU_RF_SLV1_FIFO_EN     0x02
#define MPU_RF_SLV0_FIFO_EN     0x01
// MPU_RA_USER_CTRL
#define MPU_RF_FIFO_EN          0x40
#define MPU_RF_I2C_MST_EN       0x20
#define MPU_RF_I2C_IF_DIS       0x10
#define MPU_RF_FIFO_RESET       0x04
#define MPU_RF_I2C_MST_RESET    0x02
#define MPU_RF_SIG_COND_RESET   0x01
// MPU_RA_INT_ENABLE
#define MPU_RF_DATA_RDY_EN (1 << 0)

#define MPU6050_SMPLRT_DIV      0       // 8000Hz

enum lpf_e {
    INV_FILTER_256HZ_NOLPF2 = 0,
    INV_FILTER_188HZ,
    INV_FILTER_98HZ,
    INV_FILTER_42HZ,
    INV_FILTER_20HZ,
    INV_FILTER_10HZ,
    INV_FILTER_5HZ,
    INV_FILTER_2100HZ_NOLPF,
    NUM_FILTER
};
enum gyro_fsr_e {
    INV_FSR_250DPS = 0,
    INV_FSR_500DPS,
    INV_FSR_1000DPS,
    INV_FSR_2000DPS,
    NUM_GYRO_FSR
};
enum clock_sel_e {
    INV_CLK_INTERNAL = 0,
    INV_CLK_PLL,
    NUM_CLK
};
enum accel_fsr_e {
    INV_FSR_2G = 0,
    INV_FSR_4G,
    INV_FSR_8G,
    INV_FSR_16G,
    NUM_ACCEL_FSR
};

static uint8_t mpuLowPassFilter = INV_FILTER_42HZ;
static void mpu6050AccInit(void);
static void mpu6050AccRead(int16_t *accData);
static void mpu6050GyroInit(void);
static void mpu6050GyroRead(int16_t *gyroData);
void mpu6050FifoEnable(void);


typedef enum {
    MPU_6050_HALF_RESOLUTION,
    MPU_6050_FULL_RESOLUTION
} mpu6050Resolution_e;


static mpu6050Resolution_e mpuAccelTrim;

static const mpu6050Config_t *mpu6050Config = NULL;

#ifdef USE_MPU_DATA_READY_SIGNAL

static extiCallbackRec_t mpu6050_extiCallbackRec;

void mpu6050_extiHandler(extiCallbackRec_t* cb)
{
    UNUSED(cb);
#ifdef DEBUG_MPU_DATA_READY_INTERRUPT
    // Measure the delta in micro seconds between calls to the interrupt handler
    static uint32_t lastCalledAt = 0;
    static int32_t callDelta = 0;

    uint32_t now = micros();
    callDelta = now - lastCalledAt;

    //UNUSED(callDelta);
    debug[0] = callDelta;

    lastCalledAt = now;
#endif
}

#endif

void mpu6050GpioInit(void) {
    static bool mpu6050GpioInitDone = false;

    if (mpu6050GpioInitDone || !mpu6050Config) {
        return;
    }


#ifdef USE_MPU_DATA_READY_SIGNAL
    IOConfigGPIO(mpu6050Config->intIO, Mode_IN_FLOATING);  // TODO - EXTIConfigGPIO()
    EXTIHandlerInit(&mpu6050_extiCallbackRec, mpu6050_extiHandler);
    EXTIConfig(mpu6050Config->intIO, &mpu6050_extiCallbackRec, NVIC_PRIO_MPU_INT_EXTI, EXTI_Trigger_Rising);
    EXTIEnable(mpu6050Config->intIO, true);
#endif

    mpu6050GpioInitDone = true;
}

static bool mpu6050Detect(void)
{
    bool ack;
    uint8_t sig;

    delay(35);          // datasheet page 13 says 30ms. other stuff could have been running meanwhile. but we'll be safe

    ack = i2cRead(MPU6050_ADDRESS, MPU_RA_WHO_AM_I, 1, &sig);
    if (!ack)
        return false;

    // So like, MPU6xxx has a "WHO_AM_I" register, that is used to verify the identity of the device.
    // The contents of WHO_AM_I are the upper 6 bits of the MPU-60X0�s 7-bit I2C address.
    // The least significant bit of the MPU-60X0�s I2C address is determined by the value of the AD0 pin. (we know that already).
    // But here's the best part: The value of the AD0 pin is not reflected in this register.

    if (sig != (MPU6050_ADDRESS & 0x7e))
        return false;

    return true;
}

bool mpu6050AccDetect(const mpu6050Config_t *configToUse, acc_t *acc)
{
    uint8_t readBuffer[6];
    uint8_t revision;
    uint8_t productId;

    mpu6050Config = configToUse;

    if (!mpu6050Detect()) {
        return false;
    }

    // There is a map of revision contained in the android source tree which is quite comprehensive and may help to understand this code
    // See https://android.googlesource.com/kernel/msm.git/+/eaf36994a3992b8f918c18e4f7411e8b2320a35f/drivers/misc/mpu6050/mldl_cfg.c

    // determine product ID and accel revision
    i2cRead(MPU6050_ADDRESS, MPU_RA_XA_OFFS_H, 6, readBuffer);
    revision = ((readBuffer[5] & 0x01) << 2) | ((readBuffer[3] & 0x01) << 1) | (readBuffer[1] & 0x01);
    if (revision) {
        /* Congrats, these parts are better. */
        if (revision == 1) {
            mpuAccelTrim = MPU_6050_HALF_RESOLUTION;
        } else if (revision == 2) {
            mpuAccelTrim = MPU_6050_FULL_RESOLUTION;
        } else {
            failureMode(5);
        }
    } else {
        i2cRead(MPU6050_ADDRESS, MPU_RA_PRODUCT_ID, 1, &productId);
        revision = productId & 0x0F;
        if (!revision) {
            failureMode(5);
        } else if (revision == 4) {
            mpuAccelTrim = MPU_6050_HALF_RESOLUTION;
        } else {
            mpuAccelTrim = MPU_6050_FULL_RESOLUTION;
        }
    }

    acc->init = mpu6050AccInit;
    acc->read = mpu6050AccRead;
    acc->revisionCode = (mpuAccelTrim == MPU_6050_HALF_RESOLUTION ? 'o' : 'n'); // es/non-es variance between MPU6050 sensors, half of the naze boards are mpu6000ES.

    return true;
}

bool mpu6050GyroDetect(const mpu6050Config_t *configToUse, gyro_t *gyro, uint16_t lpf)
{
    mpu6050Config = configToUse;

    if (!mpu6050Detect()) {
        return false;
    }


    gyro->init = mpu6050GyroInit;
    gyro->read = mpu6050GyroRead;

    // 16.4 dps/lsb scalefactor
    gyro->scale = 1.0f / 16.4f;

    if (lpf == 0xffff)            // diasable LPF if requested
        mpuLowPassFilter = INV_FILTER_256HZ_NOLPF2;
    else if (lpf >= 188)
        mpuLowPassFilter = INV_FILTER_188HZ;
    else if (lpf >= 98)
        mpuLowPassFilter = INV_FILTER_98HZ;
    else if (lpf >= 42)
        mpuLowPassFilter = INV_FILTER_42HZ;
    else if (lpf >= 20)
        mpuLowPassFilter = INV_FILTER_20HZ;
    else if (lpf >= 10)
        mpuLowPassFilter = INV_FILTER_10HZ;
    else
        mpuLowPassFilter = INV_FILTER_5HZ;

    return true;
}

static void mpu6050AccInit(void)
{
    mpu6050GpioInit();

    switch (mpuAccelTrim) {
        case MPU_6050_HALF_RESOLUTION:
            acc_1G = 256 * 8;
            break;
        case MPU_6050_FULL_RESOLUTION:
            acc_1G = 512 * 8;
            break;
    }
}

static void mpu6050AccRead(int16_t *accData)
{
    uint8_t buf[6];

    if (!i2cRead(MPU6050_ADDRESS, MPU_RA_ACCEL_XOUT_H, 6, buf)) {
        return;
    }

    accData[0] = (int16_t)((buf[0] << 8) | buf[1]);
    accData[1] = (int16_t)((buf[2] << 8) | buf[3]);
    accData[2] = (int16_t)((buf[4] << 8) | buf[5]);
}

static void mpu6050GyroInit(void)
{
    mpu6050GpioInit();

    i2cWrite(MPU6050_ADDRESS, MPU_RA_PWR_MGMT_1, 0x80);      //PWR_MGMT_1    -- DEVICE_RESET 1
    delay(100);
    i2cWrite(MPU6050_ADDRESS, MPU_RA_PWR_MGMT_1, 0x03); //PWR_MGMT_1    -- SLEEP 0; CYCLE 0; TEMP_DIS 0; CLKSEL 3 (PLL with Z Gyro reference)

    if(mpuLowPassFilter == INV_FILTER_256HZ_NOLPF2)         // keep 1khz sampling frequency if internal filter is disabled
        i2cWrite(MPU6050_ADDRESS, MPU_RA_SMPLRT_DIV, 0x07); //SMPLRT_DIV    -- SMPLRT_DIV = 7  Sample Rate = Gyroscope Output Rate / (1 + SMPLRT_DIV)
    else
        i2cWrite(MPU6050_ADDRESS, MPU_RA_SMPLRT_DIV, 0x00);
    i2cWrite(MPU6050_ADDRESS, MPU_RA_CONFIG, mpuLowPassFilter); //CONFIG        -- EXT_SYNC_SET 0 (disable input pin for data sync) ; default DLPF_CFG = 0 => ACC bandwidth = 260Hz  GYRO bandwidth = 256Hz)
    i2cWrite(MPU6050_ADDRESS, MPU_RA_GYRO_CONFIG, INV_FSR_2000DPS << 3);   //GYRO_CONFIG   -- FS_SEL = 3: Full scale set to 2000 deg/sec

    // ACC Init stuff. Moved into gyro init because the reset above would screw up accel config. Oops.
    // Accel scale 8g (4096 LSB/g)
    i2cWrite(MPU6050_ADDRESS, MPU_RA_ACCEL_CONFIG, INV_FSR_8G << 3);

    i2cWrite(MPU6050_ADDRESS, MPU_RA_INT_PIN_CFG,
            0 << 7 | 0 << 6 | 0 << 5 | 0 << 4 | 0 << 3 | 0 << 2 | 1 << 1 | 0 << 0); // INT_PIN_CFG   -- INT_LEVEL_HIGH, INT_OPEN_DIS, LATCH_INT_DIS, INT_RD_CLEAR_DIS, FSYNC_INT_LEVEL_HIGH, FSYNC_INT_DIS, I2C_BYPASS_EN, CLOCK_DIS

    // TODO ! fifo enabled here !
#ifdef ACCGYRO_FIFO
    mpu6050FifoEnable();
#endif

#ifdef USE_MPU_DATA_READY_SIGNAL
    i2cWrite(MPU6050_ADDRESS, MPU_RA_INT_ENABLE, MPU_RF_DATA_RDY_EN);
#endif
}

static void mpu6050GyroRead(int16_t *gyroData)
{
    uint8_t buf[6];

    if (!i2cRead(MPU6050_ADDRESS, MPU_RA_GYRO_XOUT_H, 6, buf)) {
        return;
    }

    gyroData[0] = (int16_t)((buf[0] << 8) | buf[1]);
    gyroData[1] = (int16_t)((buf[2] << 8) | buf[3]);
    gyroData[2] = (int16_t)((buf[4] << 8) | buf[5]);
}

int mpu6050GetFifoLen(void)
{
    uint8_t buf[2];
    i2cRead(MPU6050_ADDRESS, MPU_RA_FIFO_COUNTH, 2, buf);
    return (buf[0] << 8) | buf[1];
}

void mpu6050FifoEnable(void)
{
    i2cWrite(MPU6050_ADDRESS, MPU_RA_USER_CTRL, MPU_RF_FIFO_RESET);   // flush FIFO
    i2cWrite(MPU6050_ADDRESS, MPU_RA_USER_CTRL, MPU_RF_FIFO_EN);      // enable FIFO
    i2cWrite(MPU6050_ADDRESS, MPU_RA_FIFO_EN, MPU_RF_XG_FIFO_EN | MPU_RF_YG_FIFO_EN | MPU_RF_ZG_FIFO_EN | MPU_RF_ACCEL_FIFO_EN);
}

void mpu6050FifoFlush(void)
{
    i2cWrite(MPU6050_ADDRESS, MPU_RA_USER_CTRL, MPU_RF_FIFO_RESET);   // flush FIFO
    i2cWrite(MPU6050_ADDRESS, MPU_RA_USER_CTRL, MPU_RF_FIFO_EN);      // enable FIFO
}

int mpu6050FifoRead(uint8_t *buffer, int maxLen, int modulo) {
    int fifoLen;
    int tries=2;
    do {
        fifoLen = mpu6050GetFifoLen();
        // read length again if only partial data are in FIFO (rest of registers will be writen very soon)
        // this ensures that out-of-sync configiton is easily detected
        pinDbgToggle(DBP_MPU6050_1);
    } while(--tries && (fifoLen < maxLen && fifoLen % modulo));
    int len = MIN(maxLen, fifoLen);
    if(len>12 && len <= 36)
        pinDbgToggle(DBP_MPU6050_2);
    if(len)
        if(!i2cRead(MPU6050_ADDRESS, MPU_RA_FIFO_R_W, len, buffer))
            return -1;
    return len;
}

// return number of samples processed
int mpu6050GyroAccFetch(void)
{
    int16_t gyroAccBuffer[8][6];   // TODO - uint8_t limitation for size
    int len = mpu6050FifoRead(((uint8_t*)gyroAccBuffer), sizeof(gyroAccBuffer), 12);
    int idx = 0;
    while(len > 0) {
        if(len < 12) {
            mpu6050FifoFlush();
            return -1;
        }
        for(int i = 0; i < 6; i++)
            gyroAccBuffer[idx][i] = __builtin_bswap16(gyroAccBuffer[idx][i]);
        accHandleData(&gyroAccBuffer[idx][0]);
        gyroHandleData(&gyroAccBuffer[idx][3]);
        len -= 12; idx++;
    }
    return idx;
}
