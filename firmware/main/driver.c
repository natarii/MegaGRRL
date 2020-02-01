#include "driver.h"
#include "driver/gpio.h"
#include "xtensa/core-macros.h"
#include "mallocs.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include <string.h>
#include "esp_pm.h"
#include "vgm.h"
#include "pins.h"
#include "dacstream.h"
#include "channels.h"
#include "hal.h"
#include "driver/gpio.h"
#include "loader.h"

static const char* TAG = "Driver";

#define DRIVER_CLOCK_RATE 240000000 //clock rate of cpu while in playback
#define DRIVER_VGM_SAMPLE_RATE 44100
#define DRIVER_CYCLES_PER_SAMPLE (DRIVER_CLOCK_RATE/DRIVER_VGM_SAMPLE_RATE)

#if defined HWVER_PORTABLE
#define SR_CONTROL      0
#define SR_DATABUS      1
#define SR_BIT_PSG_CS   0x01 // PSG /CS
#define SR_BIT_WR       0x04 // /WR
#define SR_BIT_FM_CS    0x20 // FM /CS
#define SR_BIT_A0       0x08 // A0
#define SR_BIT_A1       0x10 // A1
#define SR_BIT_IC       0x02 // /IC
uint8_t Driver_SrBuf[2] = {0xff & ~SR_BIT_IC,0x00};
#elif defined HWVER_DESKTOP
#define SR_CONTROL      1
#define SR_DATABUS      0
#define SR_BIT_PSG_CS   0x40 // PSG /CS
#define SR_BIT_WR       0x20 // /WR
#define SR_BIT_FM_CS    0x80 // FM /CS
#define SR_BIT_A0       0x08 // A0
#define SR_BIT_A1       0x04 // A1
#define SR_BIT_IC       0x10 // /IC
uint8_t Driver_SrBuf[2] = {0x00,0xff & ~SR_BIT_IC};
#endif

QueueHandle_t Driver_CommandQueue; //queue of incoming vgm data
QueueHandle_t Driver_PcmQueue; //queue of attached pcm data (if any)
EventGroupHandle_t Driver_CommandEvents; //driver status flags
EventGroupHandle_t Driver_QueueEvents; //queue status flags
StaticQueue_t Driver_CommandStaticQueue;
StaticQueue_t Driver_PcmStaticQueue;
uint8_t Driver_CommandQueueBuf[DRIVER_QUEUE_SIZE];
uint8_t Driver_PcmBuf[DACSTREAM_BUF_SIZE*DACSTREAM_PRE_COUNT];

volatile uint32_t Driver_CpuPeriod = 0;
volatile uint32_t Driver_CpuUsageVgm = 0;
volatile uint32_t Driver_CpuUsageDs = 0;

volatile bool Driver_FixPsgFrequency = true;
volatile bool Driver_FixPsgPeriodic = true;
volatile bool Driver_MitigateVgmTrim = true;

volatile MegaMod_t Driver_DetectedMod = MEGAMOD_NONE;

//we abuse the esp32's spi hardware to drive the shift registers
spi_bus_config_t Driver_SpiBusConfig = {
    .miso_io_num=-1,
    .mosi_io_num=PIN_DRIVER_DATA,
    .sclk_io_num=PIN_DRIVER_SHCLK,
    .quadwp_io_num=-1,
    .quadhd_io_num=-1,
};
spi_device_interface_config_t Driver_SpiDeviceConfig = {
    .clock_speed_hz=26600000, /*incredibly loud green hill zone music plays*/
    .mode=0,
    .spics_io_num=PIN_DRIVER_SHSTO,
    .queue_size=10,
    //.flags = SPI_DEVICE_NO_DUMMY
};
spi_device_handle_t Driver_SpiDevice;

//vgm / 2612 pcm stuff
uint32_t Driver_Sample = 0;     //current sample number
uint64_t Driver_Cycle = 0;      //current cycle number
uint32_t Driver_Cc = 0;         //current cycle from the api - just keep it off the stack
uint32_t Driver_LastCc = 0;     //copy of the above var
uint32_t Driver_NextSample = 0; //sample number at which the next command needs to be run
uint8_t Driver_FmAlgo[6] = {0,0,0,0,0,0};
uint8_t Driver_PsgLastChannel = 0;
volatile bool Driver_FirstWait = true;
uint8_t Driver_FmPans[6] = {0b11000000,0b11000000,0b11000000,0b11000000,0b11000000,0b11000000};
uint32_t Driver_PauseSample = 0; //sample no before stop
uint8_t Driver_PsgAttenuation[4] = {0b10011111, 0b10111111, 0b11011111, 0b11111111};
bool Driver_NoLeds = false;

//dacstream specific
uint32_t DacStreamSeq = 0;              //sequence no of the current stream
uint32_t DacStreamLastSeqPlayed = 0;    //last seq successfully played
uint8_t DacStreamId = 0;                //index of the currently playing dacstream. should maybe consider using a pointer to the queue instead of keeping this around
bool DacStreamActive = false;           //actively playing a stream?
uint32_t DacStreamSampleRate = 0;       //stream current sample rate
uint32_t DacStreamSampleTime = 0;       //stream sample timer
uint8_t DacStreamPort = 0;              //chip port to write to
uint8_t DacStreamCommand = 0;           //chip command to use
uint32_t DacStreamSamplesPlayed = 0;    //how many samples played so far
uint8_t DacStreamLengthMode = 0;
uint32_t DacStreamDataLength = 0;
bool DacStreamFailed = false;

volatile uint8_t Driver_FmMask = 0b01111111;
volatile uint8_t Driver_PsgMask = 0b00001111;
uint8_t Driver_DacEn = 0;
volatile bool Driver_ForceMono = false;
uint8_t Driver_Opna_AdpcmConfig = 0b11000000; //todo verify in emu? 
uint8_t Driver_Opna_RhythmConfig[6] = {0b11000000,0b11000000,0b11000000,0b11000000,0b11000000,0b11000000}; //todo verify in emu
uint8_t Driver_Opna_SsgConfig = 0b00111111; //todo verify in emu

uint32_t Driver_PsgCh3Freq = 0;
bool Driver_PsgNoisePeriodic = false;
bool Driver_PsgNoiseSourceCh3 = false;

volatile uint32_t Driver_Opna_PcmUploadId = 0;
volatile bool Driver_Opna_PcmUpload = false;
FILE *Driver_Opna_PcmUploadFile = NULL;

#define min(a,b) ((a) < (b) ? (a) : (b)) //sigh.

void Driver_ResetChips();
void Driver_Sleep(uint32_t us);

void Driver_Output() { //output data to shift registers
    disp_spi_transfer_data(Driver_SpiDevice, (uint8_t*)&Driver_SrBuf, NULL, 2, 0);
}

static uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

bool Driver_Setup() {
    //create the queues and event groups
    ESP_LOGI(TAG, "Setting up");
    Driver_CommandQueue = xQueueCreateStatic(DRIVER_QUEUE_SIZE, sizeof(uint8_t), &Driver_CommandQueueBuf[0], &Driver_CommandStaticQueue);
    if (Driver_CommandQueue == NULL) {
        ESP_LOGE(TAG, "Command queue create failed !!");
        return false;
    }
    Driver_PcmQueue = xQueueCreateStatic(DRIVER_QUEUE_SIZE, sizeof(uint8_t), &Driver_PcmBuf[0], &Driver_PcmStaticQueue);
    if (Driver_PcmQueue == NULL) {
        ESP_LOGE(TAG, "Pcm queue create failed !!");
        return false;
    }
    Driver_CommandEvents = xEventGroupCreate();
    if (Driver_CommandEvents == NULL) {
        ESP_LOGE(TAG, "Command event group create failed !!");
        return false;
    }
    Driver_QueueEvents = xEventGroupCreate();
    if (Driver_QueueEvents == NULL) {
        ESP_LOGE(TAG, "Queue event group create failed !!");
        return false;
    }

    //setup the spi stuff
    ESP_LOGI(TAG, "Spi setup...");
    esp_err_t ret = spi_bus_initialize(VSPI_HOST, &Driver_SpiBusConfig, 0); //last param = dma ch.
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Spi bus init fail !! 0x%x", ret);
        return false;
    }
    ret = spi_bus_add_device(VSPI_HOST, &Driver_SpiDeviceConfig, &Driver_SpiDevice);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Spi bus device add fail !! 0x%x", ret);
        return false;
    }
    
    //spi configs are normally applied on the first call to spi_device_transmit
    //but we have a hacked library and we don't use that method so it would never be set up
    //output some dummy data just to get the config applied
    spi_transaction_t txn;
    memset(&txn, 0, sizeof(txn));
    txn.length = 16; //BITS !!
    txn.tx_buffer = &Driver_SrBuf;
    txn.rx_buffer = NULL;
    spi_device_transmit(Driver_SpiDevice, &txn);

    //and now output initial values
    Driver_Output();

    Driver_ResetChips();

    /*ESP_LOGW(TAG, "Benchmarking FmOut");
    uint32_t s = xthal_get_ccount();
    for (uint8_t i=0;i<255;i++) {
        Driver_FmOut(0,0,0);
    }
    uint32_t e = xthal_get_ccount();
    e = (e-s)/255;
    ESP_LOGW(TAG, "Average write latency %d clocks", e);*/

    ESP_LOGI(TAG, "Ready");
    return true;
}

void Driver_ModDetect() {
    gpio_config_t det;
    det.intr_type = GPIO_PIN_INTR_DISABLE;
    det.mode = GPIO_MODE_INPUT;
    det.pull_down_en = 0;
    det.pull_up_en = 1;
    det.pin_bit_mask = 1ULL<<PIN_CLK_PSG;
    gpio_config(&det);
    uint8_t foundbit = 0xff;
    uint8_t lastbit = 0xff;
    for (uint8_t attempts=0;attempts<8;attempts++) {
        for (uint8_t bit=0;bit<=7;bit++) {
            Driver_SrBuf[SR_DATABUS] = ~(1<<bit);
            Driver_Output();
            Driver_Sleep(1000);
            ESP_LOGD(TAG, "mod detect bit %d", bit);
            if (gpio_get_level(PIN_CLK_PSG) == 0) {
                ESP_LOGD(TAG, "low");
                if (lastbit != 0xff && lastbit != bit) {
                    ESP_LOGE(TAG, "Driver_ModDetect() noisy detect pin !!");
                    Driver_DetectedMod = MEGAMOD_FAULT;
                    return;
                }
                foundbit = bit;
            }
        }
    }
    if (foundbit == 0xff) { //nothing detected, it's probably PSG
        ESP_LOGI(TAG, "Driver_ModDetect() nothing found");
        Driver_DetectedMod = MEGAMOD_NONE;
        return;
    }
    Driver_DetectedMod = foundbit;
}

void Driver_Sleep(uint32_t us) { //quick and dirty spin sleep
    uint32_t s = xthal_get_ccount();
    uint32_t c = us*(DRIVER_CLOCK_RATE/1000000);
    while (xthal_get_ccount() - s < c);
}

void Driver_PsgOut(uint8_t Data) {
    //data bus is reversed for the psg because it made pcb layout easier
    Driver_SrBuf[SR_DATABUS] = 0;
    for (uint8_t i=0;i<=7;i++) {
        Driver_SrBuf[SR_DATABUS] |= ((Data>>(7-i))&1)<<i;
    }
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_PSG_CS;
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_WR;
    Driver_Output();
    Driver_Sleep(20);
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_WR;
    Driver_Output();
    //Driver_Sleep(20);
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_PSG_CS;
    Driver_Output();
    Driver_Sleep(5);

    //channel led stuff
    if (Driver_NoLeds) return;
    if (Data & 0x80) {
        Driver_PsgLastChannel = (Data & 0b01100000) >> 5;
    }
    uint8_t ch = 6 + Driver_PsgLastChannel; //6 = psg ch offset in array
    if ((Data & 0b10010000) == 0b10010000) { //attenuation
        uint8_t atten = Data & 0b00001111;
        if (atten == 0b1111) { //full atten, off.
            ChannelMgr_States[ch] &= ~CHSTATE_KON;
        } else {
            ChannelMgr_States[ch] |= CHSTATE_KON | CHSTATE_PARAM;
        }
    }
    if ((Data & 0b10010000) == 0b10000000) { //frequency low bits
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    }
    if ((Data & 0b10000000) == 0) { //frequency high bits
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    }
    if ((Data & 0b11110000) == 0b11100000) { //noise params
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    }
}

void Driver_WritePsgCh3Freq() {
    //get the current frequency value. this comes from catching register writes
    uint32_t freq = Driver_PsgCh3Freq;

    //if the fix is enabled, and psg noise is set to "periodic" mode, and it gets its freq from ch3, and ch3 is muted, then adjust ch3 freq
    if (Driver_FixPsgPeriodic && Driver_PsgNoisePeriodic && Driver_PsgNoiseSourceCh3 && Driver_PsgAttenuation[2] == 0b11011111) {
        //increase the value by 6.25%, which actually decreases output freq, because psg freq regs are "upside down"
        freq *= 10625;
        freq /= 10000;
    }

    //first byte
    uint8_t out = 0b11000000 | (freq & 0b00001111);
    Driver_PsgOut(out);

    //second byte
    out = (freq >> 4) & 0b00111111; //mask is needed to make sure overflows don't end up in psg bit 7
    Driver_PsgOut(out);
}

void Driver_ResetChips() {
        Driver_PsgOut(0b10011111);
        Driver_PsgOut(0b10111111);
        Driver_PsgOut(0b11011111);
        Driver_PsgOut(0b11111111);
        Driver_SrBuf[SR_CONTROL] ^= SR_BIT_IC;
        Driver_Output();
        Driver_Sleep(1000);
        Driver_SrBuf[SR_CONTROL] |= SR_BIT_IC;
        Driver_Output();
        Driver_Sleep(1000);
}

void Driver_FmOutopl3(uint8_t Port, uint8_t Register, uint8_t Value) {
    if (Port == 0) {
        Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_A1; //clear A1
    } else if (Port == 1) {
        Driver_SrBuf[SR_CONTROL] |= SR_BIT_A1; //set A1
    }
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_A0; //clear A0
    Driver_Output();
    Driver_Sleep(20);
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_FM_CS; // /cs low
    Driver_SrBuf[SR_DATABUS] = Register;
    Driver_Output();
    Driver_Sleep(20);
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_WR; // /wr low
    Driver_Output();
    Driver_Sleep(20);
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_WR; // /wr high
    Driver_Output();
    Driver_Sleep(20);
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_A0; //set A0
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_WR; // /wr low
    Driver_SrBuf[SR_DATABUS] = Value;
    Driver_Output();
    Driver_Sleep(20);
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_WR; // /wr high
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_FM_CS; // /cs high
    Driver_Output();
    Driver_Sleep(20);
}

void Driver_FmOutopna(uint8_t Port, uint8_t Register, uint8_t Value) {
    if (Port == 0) {
        Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_A1; //clear A1
    } else if (Port == 1) {
        Driver_SrBuf[SR_CONTROL] |= SR_BIT_A1; //set A1
    }
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_A0; //clear A0
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_FM_CS; // /cs low
    Driver_SrBuf[SR_DATABUS] = Register;
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_WR; // /wr low
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_WR; // /wr high
    Driver_Output();
    Driver_Sleep(10);
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_A0; //set A0
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_WR; // /wr low
    Driver_SrBuf[SR_DATABUS] = Value;
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_WR; // /wr high
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_FM_CS; // /cs high
    Driver_Output();

    if (!Driver_NoLeds) {
        uint8_t ch = 0;
        if (Port == 0 && Register == 0x28) { //kon
            ch = Value & 0b111;
            if (ch >= 0b100) ch = 0b11 + (ch - 0b100);
            uint8_t st = 0;
            switch (Driver_FmAlgo[ch]) {
                case 0:
                case 1:
                case 2:
                case 3:
                    st = Value & 0b10000000;
                    break;
                case 4:
                    st = Value & 0b10100000;
                    break;
                case 5:
                case 6:
                    st = Value & 0b11100000;
                    break;
                case 7:
                    st = Value & 0b11110000;
                    break;
                default: /*not possible*/
                    break;
            }
            if (st) {
                ChannelMgr_States[ch] |= CHSTATE_KON;
            } else {
                ChannelMgr_States[ch] &= ~CHSTATE_KON;
            }
        } else if (Register >= 0xb0 && Register <= 0xb2) { //algo
            Driver_FmAlgo[(Port?3:0) + Register - 0xb0] = Value >> 4;
        } else if (Register >= 0xa0 && Register <= 0xa2) { //fnum1
            ChannelMgr_States[(Port?3:0) + Register - 0xa0] |= CHSTATE_PARAM;
        } else if (Register >= 0xa4 && Register <= 0xa6) { //fnum2 + block
            ChannelMgr_States[(Port?3:0) + Register - 0xa4] |= CHSTATE_PARAM;
        } else if (Register >= 0xb0 && Register <= 0xb2) { //fb/conn
            ChannelMgr_States[(Port?3:0) + Register - 0xb0] |= CHSTATE_PARAM;
        } else if (Register >= 0xb4 && Register <= 0xb6) { //pms/ams/lr
            ChannelMgr_States[(Port?3:0) + Register - 0xb4] |= CHSTATE_PARAM;
        } else if (Register >= 0x30 && Register <= 0x9e) { //detune/mul, tl, keyscale/attack, decay/am, sustain, sustain level/release, ssg-eg
            ChannelMgr_States[(Port?3:0)+(Register%3)] |= CHSTATE_PARAM;
        } else if (Port == 0 && Register == 0x07) { //ssg tone enable
            for (uint8_t i=0;i<3;i++) { //tones
                if ((Value & (1<<i))) {
                    ChannelMgr_States[6+i] &= ~CHSTATE_KON;
                } else {
                    ChannelMgr_States[6+i] |= CHSTATE_KON;
                }
            }
            if ((Value & 0b00111000) != 0b00111000) { //glom all the noise together
                ChannelMgr_States[6+3] |= CHSTATE_KON;
            } else {
                ChannelMgr_States[6+3] &= ~CHSTATE_KON;
            }
        } else if (Port == 0 && Register >= 0x08 && Register <= 0x0a) { //level
            ch = Register - 0x08;
            ChannelMgr_States[6+ch] |= CHSTATE_PARAM;
            ChannelMgr_States[6+3] |= CHSTATE_PARAM; //implicit noise update too, todo only do this if noise is enabled on that ch
        } else if (Port == 0 && Register == 0x06) { //noise period
            ChannelMgr_States[6+3] |= CHSTATE_PARAM;
        } else if (Port == 0 && Register >= 0x00 && Register <= 0x05) { //coarse/fine tune
            ch = Register/2;
            ChannelMgr_States[6+ch] |= CHSTATE_PARAM;
        } //todo 0x08/0x0c env freq, 0x0d env wave. will need tracking of if channels have envgen enabled
    }

    if (Port == 0 && Register == 0x10) {
        Driver_Sleep(100);
    } else {
        Driver_Sleep(20);
    }
}

void Driver_Opna_PrepareUpload() {
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_A1; //set A1
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_A0; //clear A0
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_FM_CS; // /cs low
    Driver_SrBuf[SR_DATABUS] = 0x08;
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_WR; // /wr low
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_WR; // /wr high
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_FM_CS; // /cs high
    Driver_Output();
    Driver_Sleep(5);
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_A0; //set A0
}

void Driver_Opna_UploadByte(uint8_t pair) {
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_FM_CS; // /cs low
    Driver_SrBuf[SR_DATABUS] = pair;
    //Driver_Output();
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_WR; // /wr low
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_WR; // /wr high
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_FM_CS; // /cs high
    Driver_Output();
    Driver_Sleep(15);
}

void Driver_FmOut(uint8_t Port, uint8_t Register, uint8_t Value) {
    if (Port == 0) {
        Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_A1; //clear A1
    } else if (Port == 1) {
        Driver_SrBuf[SR_CONTROL] |= SR_BIT_A1; //set A1
    }
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_A0; //clear A0
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_FM_CS; // /cs low
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_WR; // /wr low
    Driver_SrBuf[SR_DATABUS] = Register;
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_WR; // /wr high
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_A0; //set A0
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_WR; // /wr low
    Driver_SrBuf[SR_DATABUS] = Value;
    Driver_Output();
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_WR; // /wr high
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_FM_CS; // /cs high
    Driver_Output();
    Driver_Sleep(5);

    //channel led stuff
    if (Driver_NoLeds) return;
    //todo: clean this up, there's so much code duplication.
    if (Register >= 0xb0 && Register <= 0xb2) {
        uint8_t ch = (Port?3:0)+(Register-0xb0);
        Driver_FmAlgo[ch] = Value & 0b111;
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Port == 0 && Register == 0x28) { //KON
        uint8_t ch = Value & 0b111;
        if (ch >= 0b100) ch = 0b11 + (ch - 0b100);
        //uint8_t st = Value >> 4;
        uint8_t st = 0;
        switch (Driver_FmAlgo[ch]) {
            case 0:
            case 1:
            case 2:
            case 3:
                st = Value & 0b10000000;
                break;
            case 4:
                st = Value & 0b10100000;
                break;
            case 5:
            case 6:
                st = Value & 0b11100000;
                break;
            case 7:
                st = Value & 0b11110000;
                break;
            default: /*not possible*/
                break;
        }
        if (st) {
            ChannelMgr_States[ch] |= CHSTATE_KON;
        } else {
            ChannelMgr_States[ch] &= ~CHSTATE_KON;
        }
    } else if (Register >= 0xa0 && Register <= 0xae) { //frequency
        uint8_t ch = (Port?3:0) + min(2,Register-0xa0); //this is wacky, but it's to deal with ch3 special mode.
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register == 0x2a) { //dac value
        if (Value >= 0x7f) {
            ChannelMgr_PcmAccu += Value - 0x7f;
        } else {
            ChannelMgr_PcmAccu += 0x7f - Value;
        }
        ChannelMgr_PcmCount++;
    } else if (Register >= 0x30 && Register <= 0x3e) { //multiply, detune
        uint8_t ch = (Port?3:0)+(Register%3);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register >= 0x40 && Register <= 0x4e) { //TL
        uint8_t ch = (Port?3:0)+(Register%3);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register >= 0x50 && Register <= 0x5e) { //attack rate/scaling
        uint8_t ch = (Port?3:0)+(Register%3);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register >= 0x60 && Register <= 0x6e) { //1st decay, am enable
        uint8_t ch = (Port?3:0)+(Register%3);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register >= 0x70 && Register <= 0x7e) { //2nd decay
        uint8_t ch = (Port?3:0)+(Register%3);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register >= 0x80 && Register <= 0x8e) { //release rate, sustain
        uint8_t ch = (Port?3:0)+(Register%3);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register >= 0x90 && Register <= 0x9e) { //SSG-EG
        uint8_t ch = (Port?3:0)+(Register%3);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register == 0x2b) { //dac enable
        if (Value & 0x80) ChannelMgr_States[5] |= CHSTATE_DAC;
        else ChannelMgr_States[5] &= ~CHSTATE_DAC;
    }
}

void Driver_UpdateCh6Muting() {
    if (Driver_MitigateVgmTrim) {
        if (Driver_FirstWait) {
            Driver_FmOut(1, 0xb6, Driver_FmPans[5] & 0b00111111);
            return;
        }
    }
    uint8_t reg = Driver_FmPans[5] & (Driver_FmMask & (1<<(Driver_DacEn?6:5))?0b11111111:0b00111111);
    if (Driver_ForceMono && (reg & 0b11000000)) reg |= 0b11000000;
    Driver_FmOut(1, 0xb6, reg);
}

uint8_t Driver_ProcessOpnaSsgWrite(uint8_t ssgreg) {
    //tones
    for (uint8_t i=0;i<3;i++) {
        if (Driver_PsgMask & (1<<i)) { //unmuted
            //do nothing
        } else { //muted
            ssgreg |= (1<<i); //force bit on (tone off)
        }
    }
    //noise
    if (Driver_PsgMask & (1<<3)) { //single bit for noise
        ssgreg |= 0b00111000;
    }
    return ssgreg;
}

void Driver_UpdateMuting() {
    if (Driver_DetectedMod == MEGAMOD_NONE) {
        if (Driver_MitigateVgmTrim) {
            if (Driver_FirstWait) {
                //force everything off no matter what
                Driver_FmOut(0, 0xb4, Driver_FmPans[0] & 0b00111111);
                Driver_FmOut(0, 0xb5, Driver_FmPans[1] & 0b00111111);
                Driver_FmOut(0, 0xb6, Driver_FmPans[2] & 0b00111111);
                Driver_FmOut(1, 0xb4, Driver_FmPans[3] & 0b00111111);
                Driver_FmOut(1, 0xb5, Driver_FmPans[4] & 0b00111111);
                Driver_FmOut(1, 0xb6, Driver_FmPans[5] & 0b00111111);
                for (uint8_t i=0;i<4;i++) {
                    Driver_PsgOut(0b10011111 | (i<<5));
                }
                return;
            }
        }
        for (uint8_t i=0;i<5;i++) {
            uint8_t mask = (Driver_FmMask & (1<<i))?0b11111111:0b00111111;
            uint8_t reg = Driver_FmPans[i] & mask;
            if (Driver_ForceMono && (reg & 0b11000000)) reg |= 0b11000000;
            Driver_FmOut((i<3)?0:1, 0xb4 + ((i<3)?i:(i-3)), reg);
        }
        Driver_UpdateCh6Muting();
        for (uint8_t i=0;i<4;i++) {
            uint8_t atten = 0;
            if (Driver_PsgMask & (1<<i)) {
                atten = Driver_PsgAttenuation[i];
            } else {
                atten = 0b10011111 | (i<<5);
            }
            Driver_PsgOut(atten);
        }
    } else if (Driver_DetectedMod == MEGAMOD_OPNA) {
        //todo: handle vgm_trim mitigation
        //todo: force mono
        //FM:
        for (uint8_t i=0;i<6;i++) {
            uint8_t mask = (Driver_FmMask & (1<<i))?0b11111111:0b00111111;
            uint8_t reg = Driver_FmPans[i] & mask;
            Driver_FmOutopna((i<3)?0:1, 0xb4 + ((i<3)?i:(i-3)), reg);
        }
        //PCM:
        Driver_FmOutopna(1, 0x01, Driver_Opna_AdpcmConfig & ((Driver_FmMask&(1<<6))?0b11111111:0b00111111));
        //rhythm:
        for (uint8_t i=0x18;i<=0x1d;i++) {
            uint8_t mask = (Driver_FmMask & (1<<6))?0b11111111:0b00111111;
            uint8_t reg = Driver_Opna_RhythmConfig[i-0x18] & mask;
            Driver_FmOutopna(0, i, reg);
        }
        //ssg:
        Driver_FmOutopna(0,0x07,Driver_ProcessOpnaSsgWrite(Driver_Opna_SsgConfig));
    }
}

void Driver_SetFirstWait() {
    Driver_FirstWait = false;
    Driver_UpdateMuting();
    Driver_Cycle = 0;
    Driver_LastCc = Driver_Cc = xthal_get_ccount();
}

uint8_t Driver_SeqToSlot(uint32_t seq) {
    for (uint8_t i=0;i<DACSTREAM_PRE_COUNT;i++) {
        if (!DacStreamEntries[i].SlotFree && DacStreamEntries[i].Seq == seq) {
            return i;
        }
    }
    ESP_LOGE(TAG, "Dacstream sync error - failed to find entry for seq %d !!", seq);
    return 0xff;
}

uint8_t Driver_PsgFreqLow = 0; //used for sega psg fix
uint16_t opnastart = 0;
uint16_t opnastart_hacked = 0;
uint16_t opnastop = 0;
uint16_t opnastop_hacked = 0;
bool Driver_RunCommand(uint8_t CommandLength) { //run the next command in the queue. command length as a parameter just to avoid looking it up a second time
    uint8_t cmd[CommandLength]; //buffer for command + attached data
    bool nw = false; //skip write
    
    //read command + data from the queue
    for (uint8_t i=0;i<CommandLength;i++) {
        xQueueReceive(Driver_CommandQueue, &cmd[i], 0);
    }

    if (cmd[0] == 0x50) { //SN76489
        //psg writes need to be intercepted to fix frequency register differences between TI PSG <-> SEGA VDP PSG
        //TODO: only do this for vgms where ver >= 1.51 and header 0x2b bit 0 is set
        //TODO: fix all the logic here - it's fucked up if fixpsgfreq = false
        if (Driver_FixPsgFrequency) {
            if ((cmd[1] & 0x80) == 0) { //ch 1~3 frequency high byte write
                if ((Driver_PsgFreqLow & 0b00001111) == 0) { //if low byte is all 0 for freq
                    if ((cmd[1] & 0b00111111) == 0) { //if high byte is all 0 for freq
                        Driver_PsgFreqLow |= 1; //set bit 0 of freq to 1
                    }
                }
                //write both registers now
                if ((Driver_PsgFreqLow & 0b01100000) == 0b01000000) { //ch3
                    Driver_PsgCh3Freq = (Driver_PsgFreqLow & 0b00001111) | ((cmd[1] & 0b00111111) << 4);
                    Driver_WritePsgCh3Freq();
                } else {
                    Driver_PsgOut(Driver_PsgFreqLow);
                    Driver_PsgOut(cmd[1]);
                }
            } else if ((cmd[1] & 0b10010000) == 0b10000000 && (cmd[1]&0b01100000)>>5 != 3) { //ch 1~3 frequency low byte write
                Driver_PsgFreqLow = cmd[1]; //just store the value for now, don't write anything until we get the high byte
            } else { //attenuation or noise ch control write
                if ((cmd[1] & 0b10010000) == 0b10010000) { //attenuation
                    uint8_t ch = (cmd[1]>>5)&0b00000011;
                    Driver_PsgAttenuation[ch] = cmd[1];
                    if (Driver_MitigateVgmTrim && Driver_FirstWait) cmd[1] |= 0b00001111; //if we haven't reached the first wait, force full attenuation
                    if ((Driver_PsgMask & (1<<ch)) == 0) {
                        cmd[1] |= 0b00001111;
                    }
                    if (ch == 2) {
                        //when ch 3 atten is updated, we also need to write frequency again. this is due to the periodic noise fix.
                        //TODO: this would be better if it only does it if actually transitioning in or out of mute, rather than on every atten update
                        Driver_WritePsgCh3Freq();
                    }
                } else if ((cmd[1] & 0b11110000) == 0b11100000) { //noise control
                    Driver_PsgNoisePeriodic = (cmd[1] & 0b00000100) == 0; //FB
                    Driver_PsgNoiseSourceCh3 = (cmd[1] & 0b00000011) == 0b00000011; //NF0, NF1
                    //periodic noise fix: update ch3 frequency when noise control settings change
                    //TODO: only update it when it matters :P
                    Driver_WritePsgCh3Freq();
                }
                Driver_PsgOut(cmd[1]);
            }
        } else { //not fixing psg frequency
            Driver_PsgOut(cmd[1]); //just write it normally
        }
    } else if (cmd[0] == 0x51) {
        Driver_FmOutopl3(0, cmd[1], cmd[2]);
    } else if (cmd[0] == 0x5e || cmd[0] == 0x5b || cmd[0] == 0x5a) { //ymf262 port 0, ym3812, ym3526
        Driver_FmOutopl3(0, cmd[1], cmd[2]);
    } else if (cmd[0] == 0x5f) { //ymf262 port 1
        Driver_FmOutopl3(1, cmd[1], cmd[2]);
    } else if (cmd[0] == 0x56 || cmd[0] == 0x57) { //opna
        //using longer delays here because writes are missing (?)
        //todo look into this further
        if (cmd[0] == 0x57) {
            if (cmd[1] == 0x01) { //control/config
                cmd[2] &= 0b11111100; //always force type=DRAM and width=1bit. even store it this way in the register backup! that way we don't have to mask it off every single time...
                Driver_Opna_AdpcmConfig = cmd[2];
                //todo vgm_trim mitigation
                //todo force mono
                cmd[2] &= (Driver_FmMask&(1<<6))?0b11111111:0b00111111; //muting mask
            } else if (cmd[1] == 0x02) { //start L
                ESP_LOGD(TAG, "start    %02x", cmd[2]);
                opnastart = (opnastart & 0xff00) | cmd[2];
                nw = true;
            } else if (cmd[1] == 0x03) { //start H
                ESP_LOGD(TAG, "start  %02x", cmd[2]);
                opnastart = (((uint16_t)cmd[2])<<8) | (opnastart & 0xff);
                nw = true;
            } else if (cmd[1] == 0x04) { //stop L
                ESP_LOGD(TAG, "stop     %02x", cmd[2]);
                opnastop = (opnastop & 0xff00) | cmd[2];
                nw = true;
            } else if (cmd[1] == 0x05) { //stop H
                ESP_LOGD(TAG, "stop   %02x", cmd[2]);
                opnastop = (((uint16_t)cmd[2])<<8) | (opnastop & 0xff);
                nw = true;
            } else if (cmd[1] == 0x00 && cmd[2] & 0x80) { //pcm start
                if (Driver_Opna_AdpcmConfig & 0b00000011) { //if in rom or 8bit dram mode, convert addresses
                    ESP_LOGD(TAG, "converting opna pcm addresses");
                    opnastart_hacked = opnastart * 8;
                    Driver_FmOutopna(1,0x02,opnastart_hacked&0xff);
                    Driver_FmOutopna(1,0x03,opnastart_hacked>>8);
                    opnastop_hacked = opnastop * 8;
                    opnastop_hacked |= 0b00000111;
                    Driver_FmOutopna(1,0x04,opnastop_hacked&0xff);
                    Driver_FmOutopna(1,0x05,opnastop_hacked>>8);
                } else { //write as-is
                    Driver_FmOutopna(1,0x02,opnastart&0xff);
                    Driver_FmOutopna(1,0x03,opnastart>>8);
                    Driver_FmOutopna(1,0x04,opnastop&0xff);
                    Driver_FmOutopna(1,0x05,opnastop>>8);
                }
            } else if (cmd[1] == 0x0c || cmd[1] == 0x0d) { //limit
                nw = true;
            }
        }
        if (cmd[1] >= 0xb4 && cmd[1] <= 0xb6) { //pan, AMS, PMS
            //todo vgm_trim mitigation
            //todo force mono
            uint8_t i = ((cmd[0]&1)?3:0)+cmd[1]-0xb4;
            Driver_FmPans[i] = cmd[2];
            cmd[2] &= (Driver_FmMask & (1<<i))?0b11111111:0b00111111;
        } else if (cmd[0] == 0x56 && cmd[1] >= 0x18 && cmd[1] <= 0x1d) { //rhythm pan/level
            //todo vgm_trim mitigation
            //todo force mono
            uint8_t i = cmd[1]-0x18;
            Driver_Opna_RhythmConfig[i] = cmd[2];
            cmd[2] &= (Driver_FmMask & (1<<6))?0b11111111:0b00111111;
        } else if (cmd[0] == 0x56 && cmd[1] == 0x07) { //ssg tone enable
            //todo vgm_trim mitigation
            //todo force mono
            Driver_Opna_SsgConfig = cmd[2];
            cmd[2] = Driver_ProcessOpnaSsgWrite(cmd[2]); //this handles masks
        }
        if (!nw) Driver_FmOutopna(cmd[0]&1, cmd[1], cmd[2]);
    } else if (cmd[0] == 0x55) { //ym2203
        Driver_FmOutopna(0, cmd[1], cmd[2]);
    } else if (cmd[0] == 0x52) { //YM2612 port 0
        if (cmd[1] >= 0xb4 && cmd[1] <= 0xb6) { //pan, FMS, AMS
            Driver_FmPans[cmd[1]-0xb4] = cmd[2];
            if (Driver_MitigateVgmTrim && Driver_FirstWait) cmd[2] &= 0b00111111; //if we haven't reached the first wait, disable both L and R
            cmd[2] &= (Driver_FmMask & (1<<(cmd[1]-0xb4)))?0b11111111:0b00111111;
            if (Driver_ForceMono && (cmd[2] & 0b11000000)) cmd[2] |= 0b11000000;
        }
        if (cmd[1] == 0x2b && (cmd[2] & 0x80) != Driver_DacEn) {
            Driver_DacEn = cmd[2] & 0x80; //we shouldn't have to mask off the other bits but who knows what kind of crazy shit games do
            ESP_LOGD(TAG, "dac mode change %02x", Driver_DacEn);
            Driver_UpdateCh6Muting();
        }
        Driver_FmOut(0, cmd[1], cmd[2]);
    } else if (cmd[0] == 0x53) { //YM2612 port 1
        if (cmd[1] >= 0xb4 && cmd[1] <= 0xb6) { //pan, FMS, AMS
            Driver_FmPans[3+cmd[1]-0xb4] = cmd[2];
            if (cmd[1] == 0xb6) { //ch6, we need to check if it's in dac mode or not
                if (Driver_DacEn) {
                    cmd[2] &= (Driver_FmMask & (1<<6))?0b11111111:0b00111111;
                } else {
                    cmd[2] &= (Driver_FmMask & (1<<5))?0b11111111:0b00111111;
                }
            } else { //otherwise apply muting masks as normal
                cmd[2] &= (Driver_FmMask & (1<<(3+(cmd[1]-0xb4))))?0b11111111:0b00111111;
            }
            if (Driver_MitigateVgmTrim && Driver_FirstWait) cmd[2] &= 0b00111111; //if we haven't reached the first wait, disable both L and R. do this after the muting logic
            if (Driver_ForceMono && (cmd[2] & 0b11000000)) cmd[2] |= 0b11000000;
        }
        Driver_FmOut(1, cmd[1], cmd[2]);
    } else if (cmd[0] == 0x61) { //16bit wait
        Driver_NextSample += *(uint16_t*)&cmd[1];
        if (Driver_FirstWait && *(uint16_t*)&cmd[1] > 0) {
            Driver_SetFirstWait();
        }
    } else if (cmd[0] == 0x62) { //60Hz wait
        Driver_NextSample += 735;
        if (Driver_FirstWait) Driver_SetFirstWait();
    } else if (cmd[0] == 0x63) { //50Hz wait
        Driver_NextSample += 882;
        if (Driver_FirstWait) Driver_SetFirstWait();
    } else if ((cmd[0] & 0xf0) == 0x70) { //4bit wait
        Driver_NextSample += (cmd[0] & 0x0f) + 1;
        if (Driver_FirstWait) Driver_SetFirstWait();
    } else if ((cmd[0] & 0xf0) == 0x80) { //YM2612 DAC + wait
        uint8_t sample;
        //TODO check if queue is empty
        xQueueReceive(Driver_PcmQueue, &sample, 0);
        Driver_FmOut(0, 0x2a, sample);
        Driver_NextSample += cmd[0] & 0x0f;
        if (Driver_FirstWait && (cmd[0] & 0x0f) > 0) {
            Driver_SetFirstWait();
        }
    } else if (cmd[0] == 0x93 || cmd[0] == 0x95) { //dac stream start
        DacStreamSeq++;
        if (DacStreamLastSeqPlayed > 0) {
            for (uint32_t s=DacStreamLastSeqPlayed;s<DacStreamSeq;s++) {
                uint8_t id = Driver_SeqToSlot(s);
                if (id != 0xff) {
                    DacStreamEntries[id].SlotFree = true;
                }
            }
        }
        uint8_t id;
        id = Driver_SeqToSlot(DacStreamSeq);
        if (id == 0xff) {
            ESP_LOGW(TAG, "DacStreamEntries under !!");
        } else {
            DacStreamId = id;
            DacStreamSampleRate = DacStreamEntries[DacStreamId].SampleRate;
            DacStreamPort = DacStreamEntries[DacStreamId].ChipPort;
            DacStreamCommand = DacStreamEntries[DacStreamId].ChipCommand;
            DacStreamSampleTime = xthal_get_ccount();
            DacStreamLastSeqPlayed = DacStreamSeq;
            DacStreamSamplesPlayed = 0;
            DacStreamLengthMode = DacStreamEntries[DacStreamId].LengthMode;
            DacStreamDataLength = DacStreamEntries[DacStreamId].DataLength;
            ESP_LOGD(TAG, "playing %d q size %d rate %d LM %d len %d", DacStreamSeq, uxQueueMessagesWaiting(DacStreamEntries[DacStreamId].Queue), DacStreamSampleRate, DacStreamLengthMode, DacStreamDataLength);
            DacStreamActive = true;
        }
    } else if (cmd[0] == 0x94) { //dac stream stop
        DacStreamActive = false;
    } else if (cmd[0] == 0x92) { //set sample rate
        if (DacStreamActive) {
            DacStreamSampleRate = *(uint32_t*)&cmd[2];
            ESP_LOGD(TAG, "Dacstream samplerate updated to %d", DacStreamSampleRate);
        } else {
            ESP_LOGD(TAG, "Not updating dacstream samplerate, not playing");
        }
    } else if (cmd[0] == 0x90 || cmd[0] == 0x91) { //dacstream commands we don't need to worry about here

    } else if (cmd[0] == 0x4f) { //gamegear psg stereo
        ESP_LOGW(TAG, "Game Gear PSG stereo not implemented !!");
    } else if (cmd[0] == 0xb2) {
        if (cmd[1] & 0b11000000) ESP_LOGW(TAG, "Unsupported 32x pwm reg write %d !!", cmd[1]>>4);
    } else if (cmd[0] == 0x66) { //end of music
        xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING);
        xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_FINISHED);
        Driver_ResetChips();
        ESP_LOGI(TAG, "reached end of music");
    } else {
        ESP_LOGE(TAG, "driver unknown command %02x !!", cmd[0]);
        return false;
    }
    return true;
}

uint32_t Driver_BusyStart = 0;
//uint32_t Driver_BusyEnd = 0;
void Driver_Main() {
    //driver task. never pet watchdog or come up for air at all - nothing else is running on CPU1.
    ESP_LOGI(TAG, "Task start");
    while (1) {
        Driver_Cc = xthal_get_ccount();
        uint8_t commandeventbits = xEventGroupGetBits(Driver_CommandEvents);
        uint8_t queueeventbits = xEventGroupGetBits(Driver_QueueEvents);
        if (commandeventbits & DRIVER_EVENT_START_REQUEST) {
            //reset all internal vars
            Driver_Sample = 0;
            Driver_Cycle = 0;
            Driver_LastCc = Driver_Cc;
            Driver_NextSample = 0;
            DacStreamActive = false;
            DacStreamSeq = 0;
            memset(&Driver_FmAlgo[0], 0, sizeof(Driver_FmAlgo));
            Driver_DacEn = 0;
            Driver_PsgLastChannel = 0; //psg can't really be reset, so technically this is kinda wrong? but it's consistent.
            Driver_FirstWait = true;
            memset(&Driver_FmPans[0], 0b11000000, sizeof(Driver_FmPans));
            Driver_Opna_AdpcmConfig = 0b11000000; //todo verify in emu?
            memset(&Driver_Opna_RhythmConfig[0], 0b11000000, sizeof(Driver_Opna_RhythmConfig)); //todo verify in emu
            Driver_Opna_SsgConfig = 0b00111111; //todo verify in emu
            for (uint8_t i=0;i<4;i++) {
                Driver_PsgAttenuation[i] = 0b10011111 | (i<<5);
            }
            Driver_UpdateMuting();
            memset(&ChannelMgr_States[0], 0, 6+4);

            //update status flags
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_FINISHED);
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_START_REQUEST);
            xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING);
            commandeventbits &= ~DRIVER_EVENT_START_REQUEST;
            commandeventbits |= DRIVER_EVENT_RUNNING;
        } else if (commandeventbits & DRIVER_EVENT_UPDATE_MUTING) {
            ESP_LOGI(TAG, "updating muting upon request");
            Driver_UpdateMuting();
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_UPDATE_MUTING);
            commandeventbits &= ~DRIVER_EVENT_UPDATE_MUTING;
        } else if (commandeventbits & DRIVER_EVENT_RESET_REQUEST) {
            Driver_ResetChips();
            for (uint8_t i=0;i<6+4;i++) {
                ChannelMgr_States[i] = 0;
            }
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_FINISHED);
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_RESET_REQUEST);
            xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RESET_ACK);
            commandeventbits &= ~DRIVER_EVENT_RESET_REQUEST;
            commandeventbits |= DRIVER_EVENT_RESET_ACK;
        } else if (commandeventbits & DRIVER_EVENT_STOP_REQUEST) {
            Driver_PauseSample = Driver_Sample;
            Driver_NoLeds = true;
            if (Driver_DetectedMod == MEGAMOD_NONE) {
                Driver_FmOut(0, 0xb4, Driver_FmPans[0] & 0b00111111);
                Driver_FmOut(0, 0xb5, Driver_FmPans[1] & 0b00111111);
                Driver_FmOut(0, 0xb6, Driver_FmPans[2] & 0b00111111);
                Driver_FmOut(1, 0xb4, Driver_FmPans[3] & 0b00111111);
                Driver_FmOut(1, 0xb5, Driver_FmPans[4] & 0b00111111);
                Driver_FmOut(1, 0xb6, Driver_FmPans[5] & 0b00111111);
                for (uint8_t i=0;i<4;i++) {
                    Driver_PsgOut(0b10011111 | (i<<5));
                }
            }
            Driver_NoLeds = false;
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_FINISHED);
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING);
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_STOP_REQUEST);
            commandeventbits &= ~(DRIVER_EVENT_RUNNING | DRIVER_EVENT_STOP_REQUEST);
        } else if (commandeventbits & DRIVER_EVENT_RESUME_REQUEST) {
            //todo: what if higher up resumes before stopping?
            Driver_NoLeds = true;
            Driver_UpdateMuting();
            Driver_NoLeds = false;
            Driver_Cc = Driver_LastCc = DacStreamSampleTime = xthal_get_ccount(); //dacstreams may end up being off by a sample or two upon resume - not going to worry about it
            Driver_Sample = Driver_PauseSample;
            xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING);
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_RESUME_REQUEST);
            commandeventbits &= ~DRIVER_EVENT_RESUME_REQUEST;
            commandeventbits |= DRIVER_EVENT_RUNNING;
        } else if (commandeventbits & DRIVER_EVENT_RUNNING) {
            Driver_Cycle += (Driver_Cc - Driver_LastCc);
            Driver_LastCc = Driver_Cc;
            Driver_Sample = Driver_Cycle / DRIVER_CYCLES_PER_SAMPLE;
            
            //vgm stuff
            if (Driver_Sample >= Driver_NextSample) { //time to move on to the next sample
                Driver_BusyStart = xthal_get_ccount();
                uint32_t waiting = uxQueueMessagesWaiting(Driver_CommandQueue);
                if (waiting > 0) { //is any data available?
                    //update half-empty event bit
                    if ((queueeventbits & DRIVER_EVENT_COMMAND_HALF) == 0 && waiting <= DRIVER_QUEUE_SIZE/2) {
                        xEventGroupSetBits(Driver_QueueEvents, DRIVER_EVENT_COMMAND_HALF);
                        queueeventbits |= DRIVER_EVENT_COMMAND_HALF;
                    } else if ((queueeventbits & DRIVER_EVENT_COMMAND_HALF) && waiting > DRIVER_QUEUE_SIZE/2) {
                        xEventGroupClearBits(Driver_QueueEvents, DRIVER_EVENT_COMMAND_HALF);
                        queueeventbits &= ~DRIVER_EVENT_COMMAND_HALF;
                    }

                    uint8_t peeked;
                    xQueuePeek(Driver_CommandQueue, &peeked, 0); //peek at first command in the queue
                    uint8_t cmdlen = VgmCommandLength(peeked); //look up the length of this command + attached data
                    if (waiting >= cmdlen) { //if the entire command + data is in the queue
                        bool ret = Driver_RunCommand(cmdlen);
                        if (!ret) {
                            printf("ERR command run fail");
                            fflush(stdout);
                            /*xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_ERROR);
                            commandeventbits |= DRIVER_EVENT_ERROR;*/
                        }
                    } else { //not enough data in queue - underrun
                        xEventGroupSetBits(Driver_QueueEvents, DRIVER_EVENT_COMMAND_UNDERRUN);
                        queueeventbits |= DRIVER_EVENT_COMMAND_UNDERRUN;
                        printf("UNDER data\n");
                        fflush(stdout);
                    }
                } else { //no data at all in queue - underrun
                    xEventGroupSetBits(Driver_QueueEvents, DRIVER_EVENT_COMMAND_UNDERRUN);
                    queueeventbits |= DRIVER_EVENT_COMMAND_UNDERRUN;
                    printf("UNDER none\n");
                    fflush(stdout);
                }
                Driver_CpuUsageVgm += (xthal_get_ccount() - Driver_BusyStart);
            } else {
                //not time for next sample yet
                if (Driver_NextSample - Driver_Sample > 2000 && !DacStreamActive) vTaskDelay(2);
            }

            //dacstream stuff
            if (DacStreamActive) {
                //todo: gracefully handle the end of a stream
                //can't just go by bytes played because some play modes are based on time
                //decide whether those are worth implementing
                Driver_BusyStart = xthal_get_ccount();
                if (uxQueueMessagesWaiting(DacStreamEntries[DacStreamId].Queue)) {
                    if (xthal_get_ccount() - DacStreamSampleTime >= (DRIVER_CLOCK_RATE/DacStreamSampleRate)) {
                        uint8_t sample;
                        xQueueReceive(DacStreamEntries[DacStreamId].Queue, &sample, 0);
                        Driver_FmOut(DacStreamPort, DacStreamCommand, sample);
                        DacStreamSampleTime += (DRIVER_CLOCK_RATE/DacStreamSampleRate);
                        DacStreamSamplesPlayed++;
                        if (DacStreamSamplesPlayed == DacStreamDataLength && (DacStreamLengthMode == 0 || DacStreamLengthMode == 1 || DacStreamLengthMode == 3)) {
                            DacStreamActive = false;
                        }
                        DacStreamFailed = false;
                    }
                } else {
                    if (!DacStreamFailed) {
                        ESP_LOGW(TAG, "DacStream sample queue under !! pos %d length %d", DacStreamSamplesPlayed, DacStreamDataLength);
                        DacStreamFailed = true;
                    }
                    //DacStreamActive = false;
                }
                Driver_CpuUsageDs += (xthal_get_ccount() - Driver_BusyStart);
            }
        } else { //not running
            if (Driver_Opna_PcmUpload) { //loader trying to upload a pcm datablock
                if (Driver_Opna_PcmUploadFile) {
                    fseek(Driver_Opna_PcmUploadFile,Loader_VgmDataBlocks[Driver_Opna_PcmUploadId].Offset,SEEK_SET);
                    uint32_t pcmoff = Loader_VgmDataBlocks[Driver_Opna_PcmUploadId].StartAddress;
                    uint32_t pcmsize = Loader_VgmDataBlocks[Driver_Opna_PcmUploadId].Size-8;

                    //TODO ALLOW FOR CHUNKS BIGGER THAN 64KBYTE
                    pcmoff /= 4;
                    Driver_FmOutopl3(1,0x02,pcmoff&0xff);   //start L
                    Driver_FmOutopl3(1,0x03,pcmoff>>8);     //start H
                    Driver_FmOutopl3(1,0x04,0xff);          //stop L
                    Driver_FmOutopl3(1,0x05,0xff);          //stop H
                    Driver_FmOutopl3(1,0x0c,0xff);          //limit L
                    Driver_FmOutopl3(1,0x0d,0xff);          //limit H
                    Driver_FmOutopl3(1,0x00,0x01);          //ADPCM reg0 reset
                    Driver_FmOutopl3(1,0x00,0x60);          //ADPCM reg0 REC | MEMDATA
                    //no need to set adpcm reg1 at this point, it should be zeroed from the reset
                    Driver_Opna_PrepareUpload();
                    for (uint32_t i=0;i<pcmsize;i++) {
                        uint8_t b;
                        fread(&b, 1, 1, Driver_Opna_PcmUploadFile);
                        Driver_Opna_UploadByte(b);
                        for (uint8_t j=0;j<=map(i,0,pcmsize,0,6);j++) {
                            ChannelMgr_States[j] |= CHSTATE_PARAM | CHSTATE_KON;
                        }
                    }
                    for (uint8_t j=0;j<6;j++) {
                        ChannelMgr_States[j] = 0;
                    }
                    Driver_FmOutopl3(1,0x00,0x01);          //ADPCM reg0 reset

                    Driver_Opna_PcmUpload = false;
                } else {
                    ESP_LOGE(TAG, "OPNA PCM upload: No file specified!!");
                }
            }

            vTaskDelay(2);
        }
    }
}