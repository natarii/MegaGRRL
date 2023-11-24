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
#include "player.h"
#include "clk.h"

static const char* TAG = "Driver";

#define DRIVER_CLOCK_RATE 240000000 //clock rate of cpu while in playback
#define DRIVER_VGM_SAMPLE_RATE 44100
#define DRIVER_CYCLES_PER_SAMPLE (DRIVER_CLOCK_RATE/DRIVER_VGM_SAMPLE_RATE)

#if defined HWVER_PORTABLE
#define SR_CONTROL      0
#define SR_DATABUS      1
#define SR_BIT_DCSG_CS   0x01 // DCSG /CS
#define SR_BIT_WR       0x04 // /WR
#define SR_BIT_FM_CS    0x20 // FM /CS
#define SR_BIT_A0       0x08 // A0
#define SR_BIT_A1       0x10 // A1
#define SR_BIT_IC       0x02 // /IC
uint8_t Driver_SrBuf[2] = {0xff & ~SR_BIT_IC,0x00};
#elif defined HWVER_DESKTOP
#define SR_CONTROL      1
#define SR_DATABUS      0
#define SR_BIT_DCSG_CS   0x40 // DCSG /CS
#define SR_BIT_WR       0x20 // /WR
#define SR_BIT_FM_CS    0x80 // FM /CS
#define SR_BIT_A0       0x08 // A0
#define SR_BIT_A1       0x04 // A1
#define SR_BIT_IC       0x10 // /IC
uint8_t Driver_SrBuf[2] = {0x00,0xff & ~SR_BIT_IC};
#endif

static portMUX_TYPE mux;

MegaStreamContext_t Driver_CommandStream; //queue of incoming vgm data
MegaStreamContext_t Driver_PcmStream; //queue of attached pcm data (if any)
EventGroupHandle_t Driver_CommandEvents; //driver status flags
EventGroupHandle_t Driver_StreamEvents; //queue status flags
uint8_t *Driver_CommandStreamBuf;
uint8_t Driver_PcmBuf[DACSTREAM_BUF_SIZE*DACSTREAM_PRE_COUNT];

volatile IRAM_ATTR uint32_t Driver_CpuPeriod = 0;
volatile IRAM_ATTR uint32_t Driver_CpuUsageVgm = 0;
volatile IRAM_ATTR uint32_t Driver_CpuUsageDs = 0;

volatile bool Driver_AssumeSegaDcsg = true;
volatile bool Driver_MitigateVgmTrim = true;
volatile uint8_t Driver_VgmDcsgSrWidth = 16;
volatile bool Driver_VgmDcsgSpecialFreq0 = true; //true = sega

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
IRAM_ATTR uint32_t Driver_Sample = 0;     //current sample number
IRAM_ATTR uint32_t Driver_Sample_Ds = 0;  //current sample number for dacstreams
uint64_t Driver_Cycle = 0;      //current cycle number
uint64_t Driver_Cycle_Ds = 0;   //current cycle number for dacstreams
IRAM_ATTR int32_t Driver_Slip = 0; //originally intended as a sample counter, currently only used as a flag.
IRAM_ATTR uint32_t Driver_Cc = 0;         //current cycle from the api - just keep it off the stack
IRAM_ATTR uint32_t Driver_LastCc = 0;     //copy of the above var
IRAM_ATTR uint32_t Driver_NextSample = 0; //sample number at which the next command needs to be run
IRAM_ATTR uint32_t Driver_ICycle = 0;
uint8_t Driver_FmAlgo[6] = {0,0,0,0,0,0};
volatile bool Driver_FirstWait = true;
uint8_t Driver_FmPans[6] = {0b11000000,0b11000000,0b11000000,0b11000000,0b11000000,0b11000000};
IRAM_ATTR uint32_t Driver_PauseSample = 0; //sample no before stop
IRAM_ATTR uint32_t Driver_PauseSample_Ds = 0; //sample no before stop for dacstreams
uint8_t Driver_DcsgAttenuation[4] = {0b10011111, 0b10111111, 0b11011111, 0b11111111};
bool Driver_NoLeds = false;
bool Driver_DcsgNoisePeriodic = false;
bool Driver_DcsgNoiseSourceCh3 = false;
static uint32_t dcsg_freq[3] = {0,0,0};
static uint8_t dcsg_latched_ch = 0;

//dacstream specific
IRAM_ATTR uint32_t DacStreamSeq = 0;              //sequence no of the current stream
IRAM_ATTR uint32_t DacStreamLastSeqPlayed = 0;    //last seq successfully played
uint8_t DacStreamId = 0;                //index of the currently playing dacstream. should maybe consider using a pointer to the queue instead of keeping this around
bool DacStreamActive = false;           //actively playing a stream?
IRAM_ATTR uint32_t DacStreamSampleRate = 0;       //stream current sample rate
uint8_t DacStreamPort = 0;              //chip port to write to
uint8_t DacStreamCommand = 0;           //chip command to use
IRAM_ATTR uint32_t DacStreamSamplesPlayed = 0;    //how many samples played so far
uint8_t DacStreamLengthMode = 0;
IRAM_ATTR uint32_t DacStreamDataLength = 0;
bool DacStreamFailed = false;

volatile uint8_t Driver_FmMask = 0b01111111;
volatile uint8_t Driver_DcsgMask = 0b00001111;
uint8_t Driver_DacEn = 0;
volatile bool Driver_ForceMono = false;
uint8_t Driver_Opna_AdpcmConfig = 0b11000000; //todo verify in emu? 
uint8_t Driver_Opna_RhythmConfig[6] = {0b11000000,0b11000000,0b11000000,0b11000000,0b11000000,0b11000000}; //todo verify in emu
uint8_t Driver_Opna_SsgConfig = 0b00111111; //todo verify in emu
uint8_t Driver_Opna_SsgLevel[3] = {0,0,0}; //todo verify in emu

volatile IRAM_ATTR uint32_t Driver_Opna_PcmUploadId = 0;
volatile bool Driver_Opna_PcmUpload = false;
FILE *Driver_Opna_PcmUploadFile = NULL;
volatile int16_t Driver_SpeedMult = 0;

static uint8_t TLs[4*6];
static uint8_t RhythmTL = 0;
static uint8_t AdpcmLevel = 0;
static const uint8_t OperatorMap[4] = {0,2,1,3};
static IRAM_ATTR uint32_t FadePos = 0;
static IRAM_ATTR uint32_t FadeStart = 0;
static IRAM_ATTR uint32_t FadeTimer = 0;
static bool FadeActive = false;
volatile bool Driver_FadeEnabled = true;
volatile uint8_t Driver_FadeLength = 3;

static uint8_t Driver_CurLoop = 0;

static uint8_t DacLastValue = 0;
static bool DacTouched = false;
static uint8_t opn2_regs_dedup[256*2];

static bool Driver_BlockOpn2TestReg = false;

static bool opn2_on_opna_mode = false;

static bool reset_flag = false;

#define min(a,b) ((a) < (b) ? (a) : (b)) //sigh.

void Driver_ResetChips(bool force);
void Driver_Sleep(uint32_t us);

void Driver_Output() { //output data to shift registers
    disp_spi_transfer_data(Driver_SpiDevice, (uint8_t*)&Driver_SrBuf, NULL, 2, 0);
}

static uint32_t map(uint32_t x, uint32_t in_min, uint32_t in_max, uint32_t out_min, uint32_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

bool Driver_Setup() {
    ESP_LOGI(TAG, "Setting up");

    vPortCPUInitializeMutex(&mux);

    ESP_LOGI(TAG, "working around dram0 size - allocating commandstream buffer...");
    Driver_CommandStreamBuf = heap_caps_malloc(DRIVER_QUEUE_SIZE, MALLOC_CAP_8BIT);
    if (Driver_CommandStreamBuf == NULL) {
        ESP_LOGE(TAG, "failed !!");
        return false;
    }
    MegaStream_Create(&Driver_CommandStream, Driver_CommandStreamBuf, DRIVER_QUEUE_SIZE);
    MegaStream_Create(&Driver_PcmStream, Driver_PcmBuf, DRIVER_QUEUE_SIZE);
    Driver_CommandEvents = xEventGroupCreate();
    if (Driver_CommandEvents == NULL) {
        ESP_LOGE(TAG, "Command event group create failed !!");
        return false;
    }
    Driver_StreamEvents = xEventGroupCreate();
    if (Driver_StreamEvents == NULL) {
        ESP_LOGE(TAG, "Stream event group create failed !!");
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

    Driver_ResetChips(true);

    /*ESP_LOGW(TAG, "Benchmarking output");
    uint32_t s = xthal_get_ccount();
    for (uint8_t i=0;i<255;i++) {
        Driver_Output();
    }
    uint32_t e = xthal_get_ccount();
    e = (e-s)/255;
    e /= 240;
    ESP_LOGW(TAG, "output time %d us", e);*/


    ESP_LOGI(TAG, "Ready");
    return true;
}




void Driver_Sleep(uint32_t us) { //quick and dirty spin sleep
    uint32_t s = xthal_get_ccount();
    uint32_t c = us*(DRIVER_CLOCK_RATE/1000000);
    while (xthal_get_ccount() - s < c);
}

void Driver_SleepClocks(uint32_t f, uint32_t clks) { //same dirty spin sleep, but timed relative to a certain clock freq and number of clocks
    uint32_t s = xthal_get_ccount();
    uint64_t c = (DRIVER_CLOCK_RATE*(uint64_t)clks)/f;
    while (xthal_get_ccount() - s < c);
}

void Driver_DcsgOut(uint8_t Data) {
    uint32_t clk = 0;
    if (Driver_DetectedMod != MEGAMOD_OPLLDCSG) { //opll+dcsg megamod only uses one clock line so this check is invalid
        clk = Clk_GetCh(1);
        if (clk == 0) return;
    } else { //we still need to know the clock for delay calculations
        clk = Clk_GetCh(0);
    }

    if (Driver_DetectedMod == MEGAMOD_OPLLDCSG) {
        #ifdef OPLLDCSG_ORIGINAL_PROTO
        Driver_SrBuf[SR_DATABUS] = 0;
        for (uint8_t i=0;i<=7;i++) {
            Driver_SrBuf[SR_DATABUS] |= ((Data>>(7-i))&1)<<i;
        }
        #else
        Driver_SrBuf[SR_DATABUS] = Data;
        #endif
    } else {
        //data bus is reversed for the dcsg because it made pcb layout easier
        Driver_SrBuf[SR_DATABUS] = 0;
        for (uint8_t i=0;i<=7;i++) {
            Driver_SrBuf[SR_DATABUS] |= ((Data>>(7-i))&1)<<i;
        }
    }

    Driver_Output();
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_DCSG_CS; //!cs low
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_WR; //!wr low
    portENTER_CRITICAL(&mux);
    Driver_Output();
    Driver_SleepClocks(clk, 36); //32, but with wiggle room. but not enough to push us into another write cycle...
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_DCSG_CS; //!cs high
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_WR; //!wr high
    Driver_Output();
    portEXIT_CRITICAL(&mux);

    //channel led stuff
    if (Driver_NoLeds) return;
    uint8_t ch = 6; //dcsg ch offset in array
    if (Data & 0x80) { //if this is not a high frequency write, then we can get the channel from the data.
        ch += (Data>>5)&3;
    } else { //it's a high frequency write so we need to pull in the last used ch
        ch += dcsg_latched_ch;
    }
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

void Driver_WriteDcsgCh3Freq() {
    //get the current frequency value. this comes from catching register writes
    uint32_t freq = dcsg_freq[2];

    //if the fix is enabled, and dcsg noise is set to "periodic" mode, and it gets its freq from ch3, and ch3 is muted, then adjust ch3 freq
    //note the ch3 mute check may need to go for strictly correct playback
    bool wants_fix = Driver_AssumeSegaDcsg || (Driver_VgmDcsgSrWidth != 15);
    if (wants_fix && Driver_DcsgNoisePeriodic && Driver_DcsgNoiseSourceCh3 && Driver_DcsgAttenuation[2] == 0b11011111) {
        if (Driver_VgmDcsgSrWidth == 16 || Driver_AssumeSegaDcsg) {
            //increase the value by 6.25%, which actually decreases output freq, because dcsg freq regs are "upside down"
            freq *= 10625;
            freq /= 10000;
        } else if (Driver_VgmDcsgSrWidth == 17) {
            freq *= 11333;
            freq /= 10000;
        }
        //no need to check any other values as they are caught and clamped in player
    }

    if (freq == 0) freq = 1;

    //first byte
    uint8_t out = 0b11000000 | (freq & 0b00001111);
    Driver_DcsgOut(out);

    //second byte
    out = (freq >> 4) & 0b00111111; //mask is needed to make sure overflows don't end up in dcsg bit 7
    Driver_DcsgOut(out);
}

void Driver_ResetChips(bool force) {
    if (reset_flag && !force) return;
    if (Driver_DetectedMod == MEGAMOD_NONE || Driver_DetectedMod == MEGAMOD_OPLLDCSG) {
        for (uint8_t i=0;i<4;i++) {
            Driver_DcsgOut(0x80 | (i<<5) | 0x10 | 0xf); //full atten
            if (i != 3) { //set freq to 0
                Driver_DcsgOut(0x80 | (i<<5));
                Driver_DcsgOut(0);
            }
        }
    }
    Driver_SrBuf[SR_CONTROL] ^= SR_BIT_IC;
    Driver_Output();
    Driver_Sleep(1000);
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_IC;
    Driver_Output();
    Driver_Sleep(1000);
    memset(opn2_regs_dedup, 0, sizeof(opn2_regs_dedup));
    opn2_regs_dedup[0xb4] = 0b11000000;
    opn2_regs_dedup[0xb5] = 0b11000000;
    opn2_regs_dedup[0xb6] = 0b11000000;
    opn2_regs_dedup[0x1b4] = 0b11000000;
    opn2_regs_dedup[0x1b5] = 0b11000000;
    opn2_regs_dedup[0x1b6] = 0b11000000;
    reset_flag = true;
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

void Driver_FmOutopll(uint8_t Register, uint8_t Value) {
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_A0; //clear A0
    Driver_Output();
    Driver_Sleep(20);
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_DCSG_CS; // /cs low
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
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_DCSG_CS; // /cs high
    Driver_Output();
    Driver_Sleep(20);
}

void Driver_FmOutpsg(uint8_t Device, uint8_t Register, uint8_t Value) {
    uint8_t csbit = SR_BIT_FM_CS;
    if (Device) {
        csbit = SR_BIT_DCSG_CS;
    }
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_A0; //clear A0
    Driver_SrBuf[SR_CONTROL] &= ~csbit; // /cs low
    Driver_SrBuf[SR_DATABUS] = Register;
    Driver_Output();
    Driver_Sleep(1);
    Driver_SrBuf[SR_CONTROL] |= SR_BIT_A0; //set A0
    Driver_Output();
    Driver_Sleep(1);
    Driver_SrBuf[SR_DATABUS] = Value;
    Driver_Output();
    Driver_Sleep(1);
    Driver_SrBuf[SR_CONTROL] |= csbit; // /cs high
    Driver_Output();
    Driver_Sleep(1);
}

void Driver_FmOutopn(uint8_t Device, uint8_t Register, uint8_t Value) {
    uint8_t csbit = SR_BIT_FM_CS;
    if (Device) {
        csbit = SR_BIT_DCSG_CS;
    }
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_A0; //clear A0
    //Driver_Output();
    Driver_SrBuf[SR_CONTROL] &= ~csbit; // /cs low
    Driver_SrBuf[SR_DATABUS] = Register;
    //Driver_Output();
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
    Driver_SrBuf[SR_CONTROL] |= csbit; // /cs high
    Driver_Output();
    if (Register <= 0x0f) {
        Driver_Sleep(1);
    } else {
        Driver_Sleep(20);
    }
}

void Driver_FmOutopna(uint8_t Port, uint8_t Register, uint8_t Value) {
    if (Port == 0) {
        Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_A1; //clear A1
    } else if (Port == 1) {
        Driver_SrBuf[SR_CONTROL] |= SR_BIT_A1; //set A1
    }
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_A0; //clear A0
    //Driver_Output();
    Driver_SrBuf[SR_CONTROL] &= ~SR_BIT_FM_CS; // /cs low
    Driver_SrBuf[SR_DATABUS] = Register;
    //Driver_Output();
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
                ChannelMgr_States[ch] |= CHSTATE_KON | CHSTATE_KON_PS;
            } else {
                ChannelMgr_States[ch] &= ~CHSTATE_KON;
            }
        } else if (Register >= 0xb0 && Register <= 0xb2) { //algo
            //Driver_FmAlgo[(Port?3:0) + Register - 0xb0] = Value >> 4;
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
                if ((Value & (1<<i)) || Driver_Opna_SsgLevel[i] == 0) {
                    ChannelMgr_States[6+i] &= ~CHSTATE_KON;
                } else {
                    ChannelMgr_States[6+i] |= CHSTATE_KON | CHSTATE_KON_PS;
                }
            }
            if ((Value & 0b00111000) != 0b00111000) { //glom all the noise together
                ChannelMgr_States[6+3] |= CHSTATE_KON | CHSTATE_KON_PS;
            } else {
                ChannelMgr_States[6+3] &= ~CHSTATE_KON;
            }
        } else if (Port == 0 && Register >= 0x08 && Register <= 0x0a) { //level
            ch = Register - 0x08;
            if ((Driver_Opna_SsgConfig & (1<<ch)) || Driver_Opna_SsgLevel[ch] == 0) { //basically the same logic as in tone enable.
                ChannelMgr_States[6+ch] &= ~CHSTATE_KON;
            } else {
                ChannelMgr_States[6+ch] |= CHSTATE_KON | CHSTATE_KON_PS;
            }
            ChannelMgr_States[6+ch] |= CHSTATE_PARAM;
            ChannelMgr_States[6+3] |= CHSTATE_PARAM; //implicit noise update too, todo only do this if noise is enabled on that ch
        } else if (Port == 0 && Register == 0x06) { //noise period
            ChannelMgr_States[6+3] |= CHSTATE_PARAM;
        } else if (Port == 0 && Register <= 0x05) { //coarse/fine tune
            ch = Register/2;
            ChannelMgr_States[6+ch] |= CHSTATE_PARAM;
        } //todo 0x08/0x0c env freq, 0x0d env wave. will need tracking of if channels have envgen enabled
    }

    if (Port == 0 && Register == 0x10) {
        Driver_Sleep(100);
    } else if (Port == 0 && Register <= 0x0f) {
        Driver_Sleep(1);
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
    Driver_Sleep(Loader_FastOpnaUpload?12:15);
}

void Driver_FmOut(uint8_t Port, uint8_t Register, uint8_t Value) {
    if (Driver_BlockOpn2TestReg) {
        if (Register == 0x21 || Register == 0x2c) return;
    }
    if (Register == 0x2a) {
        if (Driver_FirstWait || Driver_Slip) {
            DacTouched = true;
            DacLastValue = Value;
            return;
        }
        if (FadeActive) {
            int32_t sgn = Value;
            sgn -= 0x7f;
            sgn *= 1000;
            int32_t sf = map(FadePos, 0, 44100*Driver_FadeLength, 0, 155);
            sf *= sf;
            sf += 1000;
            sgn /= sf;
            sgn += 0x7f;
            Value = sgn;
        }
    }

    //we must never deduplicate writes to the low bytes of frequency. this is regs A0~A2, and in Ch3 special mode also A8~AA.
    //could explicitly check only those ranges, but it seemed that adding those checks was slowing it down further than just checking A0~AF and letting duplicate writes to the high byte go through
    if (opn2_regs_dedup[(Port<<8)|Register] != Value || (Register >> 4) == 0xa) {
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
        opn2_regs_dedup[(Port<<8)|Register] = Value;
    } else {
        return; //no led update
    }

    //channel led stuff
    if (Driver_NoLeds) return;
    //todo: clean this up, there's so much code duplication.
    if (Register >= 0xb0 && Register <= 0xb2) {
        uint8_t ch = (Port?3:0)+(Register-0xb0);
        if (ch > 5) ESP_LOGD(TAG, "algo/fb write over");
        //Driver_FmAlgo[ch] = Value & 0b111; //now handled before we get here
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Port == 0 && Register == 0x28) { //KON
        if ((Value & 3) == 3) return; //ignore bogus writes
        uint8_t ch = ((Value & 0b100)?3:0) + (Value & 0b11);
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
            ChannelMgr_States[ch] |= CHSTATE_KON | CHSTATE_KON_PS;
        } else {
            ChannelMgr_States[ch] &= ~CHSTATE_KON;
        }
    } else if (Register >= 0xa0 && Register <= 0xae) { //frequency
        uint8_t ch = (Port?3:0) + min(2,Register-0xa0); //this is wacky, but it's to deal with ch3 special mode.
        if (ch > 5) ESP_LOGD(TAG, "freq write over, reg %02x value %02x", Register, Value);
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
        if (ch > 5) ESP_LOGD(TAG, "MUL/DET write over, reg %02x value %02x", Register, Value);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register >= 0x40 && Register <= 0x4e) { //TL
        uint8_t ch = (Port?3:0)+(Register%3);
        if (ch > 5) ESP_LOGD(TAG, "TL write over, reg %02x value %02x", Register, Value);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register >= 0x50 && Register <= 0x5e) { //attack rate/scaling
        uint8_t ch = (Port?3:0)+(Register%3);
        if (ch > 5) ESP_LOGD(TAG, "AR/scale write over, reg %02x value %02x", Register, Value);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register >= 0x60 && Register <= 0x6e) { //1st decay, am enable
        uint8_t ch = (Port?3:0)+(Register%3);
        if (ch > 5) ESP_LOGD(TAG, "1st decay write over, reg %02x value %02x", Register, Value);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register >= 0x70 && Register <= 0x7e) { //2nd decay
        uint8_t ch = (Port?3:0)+(Register%3);
        if (ch > 5) ESP_LOGD(TAG, "2nd decay write over, reg %02x value %02x", Register, Value);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register >= 0x80 && Register <= 0x8e) { //release rate, sustain
        uint8_t ch = (Port?3:0)+(Register%3);
        if (ch > 5) ESP_LOGD(TAG, "release/sustain write over, reg %02x value %02x", Register, Value);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register >= 0x90 && Register <= 0x9e) { //SSG-EG
        uint8_t ch = (Port?3:0)+(Register%3);
        if (ch > 5) ESP_LOGD(TAG, "ssg-eg write over, reg %02x value %02x", Register, Value);
        ChannelMgr_States[ch] |= CHSTATE_PARAM;
    } else if (Register == 0x2b) { //dac enable
        if (Value & 0x80) ChannelMgr_States[5] |= CHSTATE_DAC;
        else ChannelMgr_States[5] &= ~CHSTATE_DAC;
    }
}

static uint8_t FadeAdpcmLevel(uint8_t tl) {
    int32_t ltl = tl;
    ltl -= map(FadePos, 0, 44100*Driver_FadeLength, 0, 0x5f);
    if (ltl < 0) ltl = 0;
    return ltl;
}

static uint8_t FilterAdpcmLevelWrite(uint8_t cmd2) {
    AdpcmLevel = cmd2;
    if (FadeActive) {
        return FadeAdpcmLevel(cmd2);
    }
    return cmd2;
}
static uint8_t FadeRhythmTL(uint8_t tl) {
    int32_t ltl = tl;
    ltl = tl & 0b00111111;
    ltl -= map(FadePos, 0, 44100*Driver_FadeLength, 0, 0x50);
    if (ltl < 0) ltl = 0;
    return ltl;
}

static uint8_t FilterRhythmTLWrite(uint8_t cmd2) {
    RhythmTL = cmd2 & 0b00111111;
    if (FadeActive) {
        return FadeRhythmTL(cmd2);
    }
    return cmd2;
}

static uint8_t FadeTL(uint8_t tl) {
    uint32_t ltl = tl;
    ltl = tl & 0b01111111;
    ltl += map(FadePos, 0, 44100*Driver_FadeLength, 0, 0x30);
    if (ltl > 0x7f) ltl = 0x7f;
    return ltl;
}

static uint8_t FilterTLWrite(uint8_t bank, uint8_t cmd1, uint8_t cmd2) {
    uint8_t op = OperatorMap[(cmd1-0x40)/4];
    uint8_t ch = (3*bank) + ((cmd1-0x40)-(OperatorMap[op]*4));
    if (op > 3 || ch > 5) ESP_LOGE(TAG, "BUG: FilterTLWrite(%d,0x%02x,0x%02x) op %d ch %d", bank, cmd1, cmd2, op, ch);
    uint8_t idx = (ch*4)+op;
    uint8_t algo = Driver_FmAlgo[ch];
    ESP_LOGD(TAG, "vgm write ch %d op %d tl %d REG 0x%02x BANK %d", ch, op, cmd2, cmd1, bank);
    TLs[idx] = cmd2 & 0b01111111;
    if (FadeActive) {
        bool scale = true;
        if (op == 0 && algo <= 6) scale = false;
        if (op == 1 && algo <= 3) scale = false;
        if (op == 2 && algo <= 4) scale = false;
        if (scale) cmd2 = FadeTL(cmd2);
    }

    return cmd2;
}

static uint8_t FadeDcsgAtten(uint8_t atten) {
    uint32_t latten = atten & 0b1111;
    latten += map(FadePos, 0, 44100*Driver_FadeLength, 0, 0xf);
    if (latten > 0xf) latten = 0xf;
    return (atten & 0b11110000) | latten;
}

static uint8_t FilterDcsgAttenWrite(uint8_t cmd1) { //handles fades and channel muting config
    uint8_t ch = (cmd1>>5) & 0b11;
    if (FadeActive) {
        cmd1 = FadeDcsgAtten(cmd1);
    }
    if ((Driver_DcsgMask & (1<<ch)) == 0) {
        cmd1 |= 0b00001111;
    }
    return cmd1;
}

static void HandleAlgoWrite(uint8_t bank, uint8_t cmd1, uint8_t cmd2) {
    uint8_t ch = (3*bank) + (cmd1-0xb0);
    if (ch > 5) ESP_LOGE(TAG, "BUG: HandleAlgoWrite(%d,0x%02x,0x%02x) ch %d", bank, cmd1, cmd2, ch);
    uint8_t oldalgo = Driver_FmAlgo[ch];
    uint8_t newalgo = cmd2 & 0b111;
    if (oldalgo != newalgo) {
        ESP_LOGD(TAG, "algo change %d -> %d", oldalgo, newalgo);
        //rewrite TLs for all ops on this ch
        for (uint8_t op=0;op<4;op++) {
            uint8_t savedtl = TLs[(ch*4)+op];
            if (FadeActive) {
                //if tl needs to be modified for this op with the new algorithm, modify it. otherwise just write the backup
                bool scale = true;
                if (op == 0 && newalgo <= 6) scale = false;
                if (op == 1 && newalgo <= 3) scale = false;
                if (op == 2 && newalgo <= 4) scale = false;
                if (scale) savedtl = FadeTL(savedtl);
            }
            ESP_LOGD(TAG, "fix write ch %d op %d tl %d REG 0x%02x BANK %d", ch, op, savedtl,  0x40+(ch%3)+(4*OperatorMap[op]), ch/3);
            if (Driver_DetectedMod == MEGAMOD_NONE) {
                Driver_FmOut(ch/3, 0x40+(ch%3)+(4*OperatorMap[op]), savedtl);
            } else if (Driver_DetectedMod == MEGAMOD_OPNA) {
                Driver_FmOutopna(ch/3, 0x40+(ch%3)+(4*OperatorMap[op]), savedtl);
            }
        }
        Driver_FmAlgo[ch] = newalgo;
    }
}

static uint8_t FadeSsgLevel(uint8_t level) {
    //todo: figure out what to do about channels with envelope enabled (bit 4)
    int32_t l = level & 0b1111;
    l -= map(FadePos,0,44100*Driver_FadeLength,0,0xf);
    if (l<0) l=0;
    return (level & 0b11110000) | l;
}

static uint8_t FilterSsgLevelWrite(uint8_t cmd1, uint8_t cmd2) { //handles fades and channel muting config - channel muting handled ONLY to stop level change popping and SSGPCM!!
    //uint8_t ch = cmd1 - 0x08;
    if (FadeActive) {
        cmd2 = FadeSsgLevel(cmd2);
    }
    return cmd2;
}

static void FadeTick() {
    if (Driver_DetectedMod == MEGAMOD_NONE || Driver_DetectedMod == MEGAMOD_OPNA) {
        for (uint8_t ch=0;ch<6;ch++) {
            for (uint8_t op=0;op<4;op++) {
                uint8_t savedtl = TLs[(ch*4)+op];
                uint8_t algo = Driver_FmAlgo[ch];
                bool scale = true;
                if (op == 0 && algo <= 6) scale = false;
                if (op == 1 && algo <= 3) scale = false;
                if (op == 2 && algo <= 4) scale = false;
                if (scale) {
                    savedtl = FadeTL(savedtl);
                    if (Driver_DetectedMod == MEGAMOD_NONE) {
                        Driver_FmOut(ch/3, 0x40+(ch%3)+(4*OperatorMap[op]), savedtl);
                    } else if (Driver_DetectedMod == MEGAMOD_OPNA) {
                        Driver_FmOutopna(ch/3, 0x40+(ch%3)+(4*OperatorMap[op]), savedtl);
                    }
                }
            }
        }
    }

    if (Driver_DetectedMod == MEGAMOD_OPNA) {
        for (uint8_t ch=0;ch<3;ch++) {
            Driver_FmOutopna(0, 0x08+ch, FilterSsgLevelWrite(0x08+ch, Driver_Opna_SsgLevel[ch]));
        }
        Driver_FmOutopna(0, 0x11, FadeRhythmTL(RhythmTL));
        Driver_FmOutopna(1, 0x0b, FadeAdpcmLevel(AdpcmLevel));
    }

    if (Driver_DetectedMod == MEGAMOD_NONE || Driver_DetectedMod == MEGAMOD_OPLLDCSG) {
        for (uint8_t ch=0;ch<4;ch++) {
            Driver_DcsgOut(FilterDcsgAttenWrite(Driver_DcsgAttenuation[ch]));
        }
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

static uint8_t Driver_ProcessSsgControlWrite(uint8_t ssgreg) {
    //tones
    for (uint8_t i=0;i<3;i++) {
        if (Driver_DcsgMask & (1<<i)) { //unmuted
            //do nothing
        } else { //muted
            ssgreg |= (1<<i); //force bit on (tone off)
        }
    }
    //noise, single bit controls all of it
    if (Driver_DcsgMask & (1<<3)) { //unmuted
        //do nothing
    } else { //muted
        ssgreg |= 0b00111000;
    }
    return ssgreg;
}

void Driver_UpdateMuting() {
    if (Driver_DetectedMod == MEGAMOD_NONE) {
        if (Driver_MitigateVgmTrim) {
            if (Driver_FirstWait || Driver_Slip) {
                //force everything off no matter what
                Driver_FmOut(0, 0xb4, Driver_FmPans[0] & 0b00111111);
                Driver_FmOut(0, 0xb5, Driver_FmPans[1] & 0b00111111);
                Driver_FmOut(0, 0xb6, Driver_FmPans[2] & 0b00111111);
                Driver_FmOut(1, 0xb4, Driver_FmPans[3] & 0b00111111);
                Driver_FmOut(1, 0xb5, Driver_FmPans[4] & 0b00111111);
                Driver_FmOut(1, 0xb6, Driver_FmPans[5] & 0b00111111);
                for (uint8_t i=0;i<4;i++) {
                    Driver_DcsgOut(0b10011111 | (i<<5));
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
            if (Driver_DcsgMask & (1<<i)) {
                atten = Driver_DcsgAttenuation[i];
            } else {
                atten = 0b10011111 | (i<<5);
            }
            Driver_DcsgOut(FilterDcsgAttenWrite(atten));
        }
    } else if (Driver_DetectedMod == MEGAMOD_OPNA) {
        //todo: handle vgm_trim mitigation
        //FM:
        for (uint8_t i=0;i<6;i++) {
            uint8_t mask = (Driver_FmMask & (1<<i))?0b11111111:0b00111111;
            uint8_t reg = Driver_FmPans[i] & mask;
            if (Driver_ForceMono && (reg & 0b11000000)) reg |= 0b11000000;
            Driver_FmOutopna((i<3)?0:1, 0xb4 + ((i<3)?i:(i-3)), reg);
        }
        //PCM:
        uint8_t adpcmreg = Driver_Opna_AdpcmConfig & ((Driver_FmMask&(1<<6))?0b11111100:0b00111100); //the lowest two bits force type=dram and width=1bit
        if (Driver_ForceMono && (adpcmreg & 0b11000000)) adpcmreg |= 0b11000000;
        Driver_FmOutopna(1, 0x01, adpcmreg);
        //rhythm:
        for (uint8_t i=0x18;i<=0x1d;i++) {
            uint8_t mask = (Driver_FmMask & (1<<6))?0b11111111:0b00111111;
            uint8_t reg = Driver_Opna_RhythmConfig[i-0x18] & mask;
            if (Driver_ForceMono && (reg & 0b11000000)) reg |= 0b11000000;
            Driver_FmOutopna(0, i, reg);
        }
        //ssg:
        //if we are coming out of mute, level registers will be wrong, because writes to them are blocked during mute. so fix them:
        for (uint8_t i=0;i<3;i++) {
            //only bother writing if that ch is actually enabled - prevents pops and led flashes
            if (Driver_DcsgMask & (1<<i)) Driver_FmOutopna(0,0x08+i,Driver_Opna_SsgLevel[i]);
        }
        //ssg tone/noise enable bits
        Driver_FmOutopna(0,0x07,Driver_ProcessSsgControlWrite(Driver_Opna_SsgConfig));
    }
}

void Driver_SetFirstWait() {
    Driver_FirstWait = false;
    Driver_UpdateMuting();
    if (DacTouched) {
        Driver_FmOut(0, 0x2a, DacLastValue);
    }
    Driver_Cycle = 0;
    Driver_ICycle = 0;
    Driver_LastCc = Driver_Cc = xthal_get_ccount();
}

typedef struct __attribute__((packed)) {
    uint16_t samples;
} mif_16bit_wait_t;

typedef struct __attribute__((packed)) {
    uint8_t reg;
    uint8_t val;
} mif_reg_val_t;

typedef struct __attribute__((packed)) {
    uint8_t stream_id;
    uint32_t sample_rate;
} mif_dacstream_sample_rate_t;

typedef union {
    mif_dacstream_sample_rate_t sample_rate;
} mif_cmd_dacstream_data_t;

typedef union {
    mif_16bit_wait_t timing_16bit_wait;
    mif_reg_val_t reg_val;
    uint8_t val;
    mif_cmd_dacstream_data_t dacstream;
} mif_cmd_data_t;

typedef struct __attribute__((packed)) {
    uint8_t cmd;
    mif_cmd_data_t data;
} mif_cmd_t;

enum mif_cmds {
    MIF_WR_DCSG_DUAL = 0x30,
    MIF_WR_PSG_OPTIONS = 0x31,
    MIF_WR_GG_STEREO = 0x4f,
    MIF_WR_DCSG = 0x50,
    MIF_WR_OPLL = 0x51,
    MIF_WR_OPN2_0 = 0x52,
    MIF_WR_OPN2_1 = 0x53,
    MIF_WR_OPM = 0x54,
    MIF_WR_OPN = 0x55,
    MIF_WR_OPNA_0 = 0x56,
    MIF_WR_OPNA_1 = 0x57,
    MIF_WR_OPNB_0 = 0x58,
    MIF_WR_OPNB_1 = 0x59,
    MIF_WR_OPL2 = 0x5a,
    MIF_WR_OPL = 0x5b,
    MIF_WR_MSX_AUDIO = 0x5c,
    MIF_WR_PCMD8 = 0x5d,
    MIF_WR_OPL3_0 = 0x5e,
    MIF_WR_OPL3_1 = 0x5f,
    MIF_16BIT_WAIT = 0x61,
    MIF_60HZ_WAIT = 0x62,
    MIF_50HZ_WAIT = 0x63,
    MIF_END = 0x66,
    MIF_4BIT_WAIT_1 = 0x70,
    MIF_4BIT_WAIT_2 = 0x71,
    MIF_4BIT_WAIT_3 = 0x72,
    MIF_4BIT_WAIT_4 = 0x73,
    MIF_4BIT_WAIT_5 = 0x74,
    MIF_4BIT_WAIT_6 = 0x75,
    MIF_4BIT_WAIT_7 = 0x76,
    MIF_4BIT_WAIT_8 = 0x77,
    MIF_4BIT_WAIT_9 = 0x78,
    MIF_4BIT_WAIT_10 = 0x79,
    MIF_4BIT_WAIT_11 = 0x7a,
    MIF_4BIT_WAIT_12 = 0x7b,
    MIF_4BIT_WAIT_13 = 0x7c,
    MIF_4BIT_WAIT_14 = 0x7d,
    MIF_4BIT_WAIT_15 = 0x7e,
    MIF_4BIT_WAIT_16 = 0x7f,
    MIF_OPN2_PCM_WAIT_0 = 0x80,
    MIF_OPN2_PCM_WAIT_1 = 0x81,
    MIF_OPN2_PCM_WAIT_2 = 0x82,
    MIF_OPN2_PCM_WAIT_3 = 0x83,
    MIF_OPN2_PCM_WAIT_4 = 0x84,
    MIF_OPN2_PCM_WAIT_5 = 0x85,
    MIF_OPN2_PCM_WAIT_6 = 0x86,
    MIF_OPN2_PCM_WAIT_7 = 0x87,
    MIF_OPN2_PCM_WAIT_8 = 0x88,
    MIF_OPN2_PCM_WAIT_9 = 0x89,
    MIF_OPN2_PCM_WAIT_10 = 0x8a,
    MIF_OPN2_PCM_WAIT_11 = 0x8b,
    MIF_OPN2_PCM_WAIT_12 = 0x8c,
    MIF_OPN2_PCM_WAIT_13 = 0x8d,
    MIF_OPN2_PCM_WAIT_14 = 0x8e,
    MIF_OPN2_PCM_WAIT_15 = 0x8f,
    MIF_DACSTREAM_SETUP = 0x90,
    MIF_DACSTREAM_SET_DATA = 0x91,
    MIF_DACSTREAM_SET_SR = 0x92,
    MIF_DACSTREAM_START = 0x93,
    MIF_DACSTREAM_STOP = 0x94,
    MIF_DACSTREAM_FASTSTART = 0x95,
    MIF_WR_PSG = 0xa0,
    MIF_WR_OPN_DUAL = 0xa5,
    MIF_WR_RF5C164 = 0xb1,
    MIF_WR_32X = 0xb2,
    MIF_WR_MSM6258 = 0xb7,
    MIF_WR_MSM6295 = 0xb8,
    MIF_WR_SCC = 0xd2,
};

static bool mif_exec_ignores(mif_cmd_t *cmd) {
    switch (cmd->cmd) {
        case MIF_WR_DCSG_DUAL:
        case MIF_WR_PSG_OPTIONS:
        case MIF_WR_GG_STEREO:
        case MIF_WR_RF5C164:
        case MIF_WR_32X:
        case MIF_WR_MSM6258:
        case MIF_WR_MSM6295:
        case MIF_WR_SCC:
            return true;
            break;
        default:
            break;
    }
    return false;
}

static void Stop() {
    ESP_LOGI(TAG, "Stopping the world...");
    Driver_ResetChips(false);
    xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING);
    xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_FINISHED);
}

static void StartFade() {
    ESP_LOGD(TAG, "Starting fadeout...");
    if (FadeActive) { //this catches cases where there are short loops and loop count is changed during the fade period
        ESP_LOGW(TAG, "Started fading, but already fading");
        return;
    }
    FadeActive = true;
    FadePos = 0; //should be redundant
    FadeStart = Driver_Sample;
    FadeTimer = Driver_Sample;
}

static bool mif_exec_timing(mif_cmd_t *cmd) {
    switch (cmd->cmd) {
        case MIF_16BIT_WAIT:
            Driver_NextSample += cmd->data.timing_16bit_wait.samples;
            if (cmd->data.timing_16bit_wait.samples == 0) return true;
            break;
        case MIF_60HZ_WAIT:
            Driver_NextSample += 735;
            break;
        case MIF_50HZ_WAIT:
            Driver_NextSample += 882;
            break;
        case MIF_4BIT_WAIT_1 ... MIF_4BIT_WAIT_16:
            Driver_NextSample += (cmd->cmd & 0x0f) + 1;
            break;
        case MIF_END:
            ESP_LOGD(TAG, "reached end of music / loop point");
            if (FadeActive) {
                ESP_LOGD(TAG, "in fadeout period, not doing anything");
            } else {
                if (Loader_VgmInfo->LoopOffset == 0 || (Loader_IgnoreZeroSampleLoops && Loader_VgmInfo->LoopSamples == 0)) { //there is no loop point at all
                    ESP_LOGI(TAG, "no loop point");
                    Stop();
                }
                if (Player_LoopCount != 255 && ++Driver_CurLoop >= Player_LoopCount) {
                    if (Driver_FadeEnabled) {
                        ESP_LOGD(TAG, "looped enough, starting fadeout");
                        StartFade();
                    } else {
                        ESP_LOGI(TAG, "Fading not enabled, just stopping");
                        Stop();
                    }
                }
                if (!Loader_IgnoreZeroSampleLoops || Loader_VgmInfo->LoopSamples > 0) {
                    ESP_LOGD(TAG, "looping");
                    if (Loader_VgmInfo->LoopSamples == 0) ESP_LOGW(TAG, "looping despite LoopSamples == 0 !!");
                }
            }
            break;
        default:
            return false;
            break;
    }
    if (Driver_FirstWait) Driver_SetFirstWait();
    return true;
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

static bool mif_exec_dacstream(mif_cmd_t *cmd) {
    switch (cmd->cmd) {
        case MIF_DACSTREAM_START:
        case MIF_DACSTREAM_FASTSTART:
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
                DacStreamLastSeqPlayed = DacStreamSeq;
                DacStreamSamplesPlayed = 0;
                Driver_Cycle_Ds = 0;
                DacStreamLengthMode = DacStreamEntries[DacStreamId].LengthMode;
                DacStreamDataLength = DacStreamEntries[DacStreamId].DataLength;
                ESP_LOGD(TAG, "playing %d q size %d rate %d LM %d len %d", DacStreamSeq, MegaStream_Used((MegaStreamContext_t *)&DacStreamEntries[DacStreamId].Stream), DacStreamSampleRate, DacStreamLengthMode, DacStreamDataLength);
                DacStreamActive = true;
            }
            return true;
            break;
        case MIF_DACSTREAM_STOP:
            DacStreamActive = false;
            return true;
            break;
        case MIF_DACSTREAM_SET_SR:
            if (DacStreamActive) {
                DacStreamSampleRate = cmd->data.dacstream.sample_rate.sample_rate;
                ESP_LOGD(TAG, "Dacstream samplerate updated to %d", DacStreamSampleRate);
                //TODO: handle the math for updating the current sample number, if sample rate changes mid-stream
            } else {
                ESP_LOGD(TAG, "Not updating dacstream samplerate, not playing");
            }
            return true;
            break;
        case MIF_DACSTREAM_SETUP:
        case MIF_DACSTREAM_SET_DATA:
            //dacstream commands we don't need to worry about
            return true;
            break;
        default:
            break;
    }
    return false;
}

static bool (*mif_exec_fn)(mif_cmd_t *) = NULL;

static void mif_special_opn2_pcm(uint8_t raw_cmd) {
    uint8_t sample;
    MegaStream_Recv(&Driver_PcmStream, &sample, 1);
    Driver_FmOut(0, 0x2a, sample);
    Driver_NextSample += raw_cmd & 0x0f;
    if (Driver_FirstWait && (raw_cmd & 0x0f) > 0) {
        Driver_SetFirstWait();
    }
}

static bool mif_exec_next_cmd(mif_cmd_t *cmd) {
    bool ret = false;
    switch (cmd->cmd >> 4) {
        //timing, control
        case 0x6:
        case 0x7:
            return mif_exec_timing(cmd);
            break;
        case 0x8:
            mif_special_opn2_pcm(cmd->cmd);
            return true;
        case 0x9:
            return mif_exec_dacstream(cmd);
            break;
        default:
            //handle fn ptr
            ret = mif_exec_fn(cmd);
            //fallback to ignores
            if (!ret) ret = mif_exec_ignores(cmd);
            return ret;
            break;
    }
    return false;
}



uint16_t opnastart = 0;
uint16_t opnastart_hacked = 0;
uint16_t opnastop = 0;
uint16_t opnastop_hacked = 0;
static void write_opna(uint8_t addr, uint8_t reg, uint8_t val) {
    bool nw = false;
    if (addr) {
        if (reg == 0x01) { //control/config
            Driver_Opna_AdpcmConfig = val; //don't force type=dram and width=1bit in the backup
            val &= 0b11111100; //but do it on the write
            //todo vgm_trim mitigation
            val &= (Driver_FmMask&(1<<6))?0b11111111:0b00111111; //muting mask
            if (Driver_ForceMono && (val & 0b11000000)) val |= 0b11000000;
        } else if (reg == 0x02) { //start L
            ESP_LOGD(TAG, "start    %02x", val);
            opnastart = (opnastart & 0xff00) | val;
            nw = true;
        } else if (reg == 0x03) { //start H
            ESP_LOGD(TAG, "start  %02x", val);
            opnastart = (((uint16_t)val)<<8) | (opnastart & 0xff);
            nw = true;
        } else if (reg == 0x04) { //stop L
            ESP_LOGD(TAG, "stop     %02x", val);
            opnastop = (opnastop & 0xff00) | val;
            nw = true;
        } else if (reg == 0x05) { //stop H
            ESP_LOGD(TAG, "stop   %02x", val);
            opnastop = (((uint16_t)val)<<8) | (opnastop & 0xff);
            nw = true;
        } else if (reg == 0x00 && val & 0x80) { //pcm start
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
            ChannelMgr_PcmAccu = 127;
            ChannelMgr_PcmCount = 1;
        } else if (reg == 0x0c || reg == 0x0d) { //limit
            nw = true;
        } else if (reg == 0x0b) { //level
            val = FilterAdpcmLevelWrite(val);
        }
    }
    if (reg >= 0xb4 && reg <= 0xb6) { //pan, AMS, PMS
        //todo vgm_trim mitigation
        uint8_t i = (addr?3:0)+reg-0xb4;
        Driver_FmPans[i] = val;
        val &= (Driver_FmMask & (1<<i))?0b11111111:0b00111111;
        if (Driver_ForceMono && (val & 0b11000000)) val |= 0b11000000;
    } else if (addr == 0 && reg >= 0x18 && reg <= 0x1d) { //rhythm pan/level
        //todo vgm_trim mitigation
        uint8_t i = reg-0x18;
        Driver_Opna_RhythmConfig[i] = val;
        val &= (Driver_FmMask & (1<<6))?0b11111111:0b00111111;
        if (Driver_ForceMono && (val & 0b11000000)) val |= 0b11000000;
    } else if (addr == 0 && reg == 0x07) { //ssg tone enable
        //todo vgm_trim mitigation
        Driver_Opna_SsgConfig = val;
        val = Driver_ProcessSsgControlWrite(val); //this handles masks
    } else if (addr == 0 && reg == 0x10) { //rhythm
        if (val & 0b00111111) {
            ChannelMgr_PcmAccu = 127;
            ChannelMgr_PcmCount = 1;
        }
    } else if (addr == 0 && reg == 0x11) { //rhythm TL
        val = FilterRhythmTLWrite(val);
    } else if (addr == 0 && reg >= 0x08 && reg <= 0x0a) {
        Driver_Opna_SsgLevel[reg - 0x08] = val;
        
        //block writes to ssg level registers during mute, to avoid level change pops:
        if ((Driver_DcsgMask & (1<<(reg-8))) == 0) nw = true;
        
        val = FilterSsgLevelWrite(reg, val);
    } else if (reg >= 0xb0 && reg <= 0xb2) { //algo
        ESP_LOGD(TAG, "algo write");
        HandleAlgoWrite(addr, reg, val);
    }
    if (((reg >= 0x40 && reg <= 0x42) || (reg >= 0x48 && reg <= 0x4a) || (reg >= 0x44 && reg <= 0x46) || (reg >= 0x4c && reg <= 0x4e))) { //TL
        ESP_LOGD(TAG, "tl write");
        val = FilterTLWrite(addr, reg, val);
    }
    if (!nw) Driver_FmOutopna(addr, reg, val); //not just checking the low bit because of opn
}

static void write_opn2(uint8_t addr, uint8_t reg, uint8_t val) {
    if (addr == 0) {
        if (Driver_DetectedMod == MEGAMOD_OPNA) { //opn2 write when opna mod detected
            if (!opn2_on_opna_mode) { //insert a command to enable 6ch mode, and enable opn2_on_opna_mode
                ESP_LOGW(TAG, "Playing OPN2 tune on OPNA hardware");
                Driver_FmOut(0, 0x29, 0x80);
                opn2_on_opna_mode = true;
            } else if (reg == 0x29) { //mask on the 6ch bit for any writes to this reg, should they exist in the vgm
                val |= 0x80;
            }
        }
        if (reg >= 0xb4 && reg <= 0xb6) { //pan, FMS, AMS
            Driver_FmPans[reg-0xb4] = val;
            if ((Driver_MitigateVgmTrim && Driver_FirstWait) || Driver_Slip) val &= 0b00111111; //if we haven't reached the first wait, disable both L and R
            val &= (Driver_FmMask & (1<<(reg-0xb4)))?0b11111111:0b00111111;
            if (Driver_ForceMono && (val & 0b11000000)) val |= 0b11000000;
        }
        if (reg == 0x2b && (val & 0x80) != Driver_DacEn) {
            Driver_DacEn = val & 0x80; //we shouldn't have to mask off the other bits but who knows what kind of crazy shit games do
            ESP_LOGD(TAG, "dac mode change %02x", Driver_DacEn);
            Driver_UpdateCh6Muting();
        }
        if ((reg >= 0x40 && reg <= 0x42) || (reg >= 0x48 && reg <= 0x4a) || (reg >= 0x44 && reg <= 0x46) || (reg >= 0x4c && reg <= 0x4e)) { //TL
            ESP_LOGD(TAG, "tl write bank0");
            val = FilterTLWrite(0, reg, val);
        }
        if (reg >= 0xb0 && reg <= 0xb2) { //algorithm
            ESP_LOGD(TAG, "algo write bank0");
            HandleAlgoWrite(0, reg, val);
        }
        Driver_FmOut(0, reg, val);
    } else {
        if (reg >= 0xb4 && reg <= 0xb6) { //pan, FMS, AMS
            Driver_FmPans[3+reg-0xb4] = val;
            if (reg == 0xb6) { //ch6, we need to check if it's in dac mode or not
                if (Driver_DacEn) {
                    val &= (Driver_FmMask & (1<<6))?0b11111111:0b00111111;
                } else {
                    val &= (Driver_FmMask & (1<<5))?0b11111111:0b00111111;
                }
            } else { //otherwise apply muting masks as normal
                val &= (Driver_FmMask & (1<<(3+(reg-0xb4))))?0b11111111:0b00111111;
            }
            if ((Driver_MitigateVgmTrim && Driver_FirstWait) || Driver_Slip) val &= 0b00111111; //if we haven't reached the first wait, disable both L and R. do this after the muting logic
            if (Driver_ForceMono && (val & 0b11000000)) val |= 0b11000000;
        }
        if ((reg >= 0x40 && reg <= 0x42) || (reg >= 0x48 && reg <= 0x4a) || (reg >= 0x44 && reg <= 0x46) || (reg >= 0x4c && reg <= 0x4e)) { //TL
            ESP_LOGD(TAG, "tl write bank1");
            val = FilterTLWrite(1, reg, val);
        }
        if (reg >= 0xb0 && reg <= 0xb2) { //algorithm
            ESP_LOGD(TAG, "algo write bank1");
            HandleAlgoWrite(1, reg, val);
        }
        Driver_FmOut(1, reg, val);
    }
}

static void write_dcsg(uint8_t val) {
    //dcsg writes need to be intercepted to fix frequency register differences between TI DCSG <-> SEGA VDP DCSG
    if ((val & 0x80) == 0) { //ch 1~3 frequency high byte write
        //note: whether or not dcsg_latched_ch is actually still in the latch on the chip (ch3 updates might blow it away) doesn't matter, because we always rewrite the low byte anyway. it's what's latched from *our* POV
        dcsg_freq[dcsg_latched_ch] = (dcsg_freq[dcsg_latched_ch] & 0b1111) | ((val & 0b111111) << 4);
        //write both registers now
        if (dcsg_latched_ch == 2) { //ch3
            Driver_WriteDcsgCh3Freq();
        } else {
            uint8_t low = 0x80 | (dcsg_latched_ch<<5) | (dcsg_freq[dcsg_latched_ch] & 0b1111);
            if (dcsg_freq[dcsg_latched_ch] == 0) low |= 1;
            Driver_DcsgOut(low);
            Driver_DcsgOut((dcsg_freq[dcsg_latched_ch] >> 4) & 0b111111);
        }
    } else if ((val & 0b10010000) == 0b10000000 && (val & 0b01100000)>>5 != 3) { //ch 1~3 frequency low byte write
        dcsg_latched_ch = (val>>5)&3;
        dcsg_freq[dcsg_latched_ch] = (dcsg_freq[dcsg_latched_ch] & 0b1111110000) | (val & 0b1111);
        //uint8_t val = cmd[1];
        if ((Driver_VgmDcsgSpecialFreq0 || Driver_AssumeSegaDcsg) && dcsg_freq[dcsg_latched_ch] == 0) {
            val |= 1;
        }
        Driver_DcsgOut(val);
    } else { //attenuation or noise ch control write
        if ((val & 0b10010000) == 0b10010000) { //attenuation
            uint8_t ch = (val>>5)&0b00000011;
            Driver_DcsgAttenuation[ch] = val;
            val = FilterDcsgAttenWrite(val);
            if ((Driver_MitigateVgmTrim && Driver_FirstWait) || Driver_Slip) val |= 0b00001111; //if we haven't reached the first wait, force full attenuation
            if (ch == 2) {
                //when ch 3 atten is updated, we also need to write frequency again. this is due to the periodic noise fix.
                //TODO: this would be better if it only does it if actually transitioning in or out of mute, rather than on every atten update
                Driver_WriteDcsgCh3Freq();
            }
        } else if ((val & 0b11110000) == 0b11100000) { //noise control
            Driver_DcsgNoisePeriodic = (val & 0b00000100) == 0; //FB
            Driver_DcsgNoiseSourceCh3 = (val & 0b00000011) == 0b00000011; //NF0, NF1
            //periodic noise fix: update ch3 frequency when noise control settings change
            //TODO: only update it when it matters :P
            Driver_WriteDcsgCh3Freq();
        }
        Driver_DcsgOut(val);
    }
}

static bool module_opl3mm_mif_exec_fn(mif_cmd_t *cmd) {
    switch (cmd->cmd) {
        case MIF_WR_OPL:
        case MIF_WR_OPL2:
        case MIF_WR_OPL3_0:
            Driver_FmOutopl3(0, cmd->data.reg_val.reg, cmd->data.reg_val.val);
            return true;
            break;
        case MIF_WR_OPL3_1:
            Driver_FmOutopl3(1, cmd->data.reg_val.reg, cmd->data.reg_val.val);
            return true;
            break;
        default:
            break;
    }
    return false;
}

static bool module_2xopnmm_mif_exec_fn(mif_cmd_t *cmd) {
    switch (cmd->cmd) {
        case MIF_WR_PSG:
            Driver_FmOutopn(cmd->data.reg_val.reg >> 7, cmd->data.reg_val.reg & ~(1<<7), cmd->data.reg_val.val);
            return true;
            break;
        case MIF_WR_OPN:
            Driver_FmOutopn(0, cmd->data.reg_val.reg, cmd->data.reg_val.val);
            return true;
            break;
        case MIF_WR_OPN_DUAL:
            Driver_FmOutopn(1, cmd->data.reg_val.reg, cmd->data.reg_val.val);
            return true;
            break;
        default:
            break;
    }
    return false;
}
static bool module_2xpsg_mif_exec_fn(mif_cmd_t *cmd) {
    switch (cmd->cmd) {
        case MIF_WR_PSG:
            Driver_FmOutpsg(cmd->data.reg_val.reg >> 7, cmd->data.reg_val.reg & ~(1<<7), cmd->data.reg_val.val);
            return true;
            break;
        default:
            break;
    }
    return false;
}

static bool module_opmmm_mif_exec_fn(mif_cmd_t *cmd) {
    switch (cmd->cmd) {
        case MIF_WR_OPM:
            Driver_FmOutopl3(0, cmd->data.reg_val.reg, cmd->data.reg_val.val); //pending proper out
            return true;
            break;
        default:
            break;
    }
    return false;
}

static bool module_opnopllmm_mif_exec_fn(mif_cmd_t *cmd) {
    switch (cmd->cmd) {
        case MIF_WR_PSG:
            //2nd psg not support
            if (cmd->data.reg_val.reg & (1<<7)) {
                return false;
            }
        case MIF_WR_OPN:
            Driver_FmOutopn(0, cmd->data.reg_val.reg, cmd->data.reg_val.val);
            return true;
            break;
        case MIF_WR_OPLL:
            Driver_FmOutopll(cmd->data.reg_val.reg, cmd->data.reg_val.val);
            break;
        default:
            break;
    }
    return false;
}

static bool module_opnamm_mif_exec_fn(mif_cmd_t *cmd) {
    switch (cmd->cmd) {
        case MIF_WR_PSG:
            //2nd psg not support
            if (cmd->data.reg_val.reg & (1<<7)) {
                return false;
            }
        case MIF_WR_OPN:
            write_opna(0, cmd->data.reg_val.reg, cmd->data.reg_val.val);
            return true;
            break;
        case MIF_WR_OPN2_0:
        case MIF_WR_OPN2_1:
            write_opn2(cmd->cmd & 1, cmd->data.reg_val.reg, cmd->data.reg_val.val);
            return true;
            break;
        case MIF_WR_OPNA_0:
        case MIF_WR_OPNA_1:
            write_opna(cmd->cmd & 1, cmd->data.reg_val.reg, cmd->data.reg_val.val);
            return true;
            break;
        default:
            break;
    }
    return false;
}

static bool module_mgdnullmm_mif_exec_fn(mif_cmd_t *cmd) {
    switch (cmd->cmd) {
        case MIF_WR_OPN2_0:
        case MIF_WR_OPN2_1:
            write_opn2(cmd->cmd & 1, cmd->data.reg_val.reg, cmd->data.reg_val.val);
            return true;
            break;
        case MIF_WR_DCSG:
            write_dcsg(cmd->data.val);
            return true;
            break;
        default:
            break;
    }
    return false;
}



void Driver_ModDetect() {
    gpio_config_t det;
    det.intr_type = GPIO_PIN_INTR_DISABLE;
    det.mode = GPIO_MODE_INPUT;
    det.pull_down_en = 0;
    det.pull_up_en = 1;
    det.pin_bit_mask = 1ULL<<PIN_CLK_DCSG;
    gpio_config(&det);
    uint8_t foundbit = 0xff;
    uint8_t lastbit = 0xff;
    for (uint8_t attempts=0;attempts<8;attempts++) {
        for (uint8_t bit=0;bit<=7;bit++) {
            Driver_SrBuf[SR_DATABUS] = ~(1<<bit);
            Driver_Output();
            Driver_Sleep(1000);
            if (gpio_get_level(PIN_CLK_DCSG) == 0) {
                ESP_LOGD(TAG, "Mod detect bit %d low", bit);
                if (lastbit != 0xff && lastbit != bit) {
                    ESP_LOGE(TAG, "Driver_ModDetect() noisy detect pin");
                    Driver_DetectedMod = MEGAMOD_FAULT;
                    return;
                }
                foundbit = bit;
            }
        }
    }
    if (foundbit == 0xff) { //nothing detected, it's probably DCSG
        ESP_LOGI(TAG, "Driver_ModDetect() nothing found");
        Driver_DetectedMod = MEGAMOD_NONE;
    }
    Driver_DetectedMod = foundbit;
    //HACK
    if (Driver_DetectedMod == MEGAMOD_OPL3) {
        mif_exec_fn = module_opl3mm_mif_exec_fn;
    } else if (Driver_DetectedMod == MEGAMOD_OPNA) {
        mif_exec_fn = module_opnamm_mif_exec_fn;
    } else if (Driver_DetectedMod == MEGAMOD_2XOPN) {
        mif_exec_fn = module_2xopnmm_mif_exec_fn;
    } else if (Driver_DetectedMod == MEGAMOD_OPNOPLL) {
        mif_exec_fn = module_opnopllmm_mif_exec_fn;
    } else if (Driver_DetectedMod == MEGAMOD_OPM) {
        mif_exec_fn = module_opmmm_mif_exec_fn;
    } else if (Driver_DetectedMod == MEGAMOD_2XPSG) {
        mif_exec_fn = module_2xpsg_mif_exec_fn;
    } else {
        mif_exec_fn = module_mgdnullmm_mif_exec_fn;
    }
}
bool Driver_RunCommand(uint8_t CommandLength) { //run the next command in the stream. command length as a parameter just to avoid looking it up a second time
    uint8_t cmd[CommandLength]; //buffer for command + attached data
    bool nw = false; //skip write
    
    //read command + data from the stream
    MegaStream_Recv(&Driver_CommandStream, cmd, CommandLength);

    if (cmd[0] == 0x50) { //SN76489
        
    } else if (cmd[0] == 0x51) {
        Driver_FmOutopll(cmd[1], cmd[2]);
    } else if (cmd[0] == 0x54) { //opm
        Driver_FmOutopl3(0, cmd[1], cmd[2]); //todo: proper timing for this
    } else if (cmd[0] == 0x5e || cmd[0] == 0x5b || cmd[0] == 0x5a) { //ymf262 port 0, ym3812, ym3526
        Driver_FmOutopl3(0, cmd[1], cmd[2]);
    } else if (cmd[0] == 0x5f) { //ymf262 port 1
        Driver_FmOutopl3(1, cmd[1], cmd[2]);
    } else if (Driver_DetectedMod == MEGAMOD_2XOPN && (cmd[0] == 0x55 || cmd[0] == 0xa5 || cmd[0] == 0xa0)) { //opn, 2nd opn, ay38910
        Driver_FmOutopn((cmd[0]&0xf0)==0xa0?1:0, cmd[1], cmd[2]);
    } else if ((Driver_DetectedMod == MEGAMOD_NONE || Driver_DetectedMod == MEGAMOD_OPNA || Driver_DetectedMod == MEGAMOD_OPNOPLL) && (cmd[0] == 0x56 || cmd[0] == 0x57 || cmd[0] == 0x55 || cmd[0] == 0xa0)) { //opna both banks, opn, AY-3-8910
        
    } else if (cmd[0] == 0x52) { //YM2612 port 0
        
    } else if (cmd[0] == 0x53) { //YM2612 port 1
        
    } else if (cmd[0] == 0x61) { //16bit wait
    } else if (cmd[0] == 0x62) { //60Hz wait
    } else if (cmd[0] == 0x63) { //50Hz wait
    } else if ((cmd[0] & 0xf0) == 0x70) { //4bit wait
    } else if ((cmd[0] & 0xf0) == 0x80) { //YM2612 DAC + wait

    } else if (cmd[0] == 0x93 || cmd[0] == 0x95) { //dac stream start
    } else if (cmd[0] == 0x94) { //dac stream stop
    } else if (cmd[0] == 0x92) { //set sample rate
    } else if (cmd[0] == 0x90 || cmd[0] == 0x91) { //dacstream commands we don't need to worry about here

    } else if (cmd[0] == 0x4f) { //gamegear dcsg stereo
        ESP_LOGD(TAG, "Game Gear DCSG stereo not implemented !!");
    } else if (cmd[0] == 0xb1) {
        //RF5C164 write, just ignore it...
    } else if (cmd[0] == 0xb2) {
        if (cmd[1] & 0b11000000) ESP_LOGW(TAG, "Unsupported 32x pwm reg write %d !!", cmd[1]>>4);
    } else if (cmd[0] == 0xc2) {
        //RF5C164 RAM write, just ignore it...
    } else if (cmd[0] == 0x66) { //end of music (loop point)
        
    } else if (cmd[0] == 0x31) {
        //ignore AY-3-8910 stereo mask
    } else if (cmd[0] == 0x30) {
        //ignore dcsg no.2
    } else if (cmd[0] == 0xb7) {
        //ignore msm6258
    } else if (cmd[0] == 0xb8) {
        //ignore msm6295
    } else if (cmd[0] == 0xd2) {
        //ignore scc
    } else if (cmd[0] == 0xff) { //receiving bad flags
        if (cmd[1] & PLAYER_BADVGM_OPN2_TESTREG) Driver_BlockOpn2TestReg = true;
    } else {
        ESP_LOGE(TAG, "driver unknown command %02x !!", cmd[0]);
        return false;
    }
    return true;
}

IRAM_ATTR uint32_t Driver_BusyStart = 0;
//uint32_t Driver_BusyEnd = 0;
void Driver_Main() {
    //driver task. never pet watchdog or come up for air at all - nothing else is running on CPU1.
    ESP_LOGI(TAG, "Task start");
    while (1) {
        Driver_Cc = xthal_get_ccount();
        EventBits_t commandeventbits = xEventGroupGetBits(Driver_CommandEvents);
        EventBits_t queueeventbits = xEventGroupGetBits(Driver_StreamEvents);
        if (commandeventbits & DRIVER_EVENT_START_REQUEST) {
            //reset all internal vars
            Driver_Cycle = 0;
            Driver_ICycle = 0;
            Driver_LastCc = Driver_Cc;
            Driver_NextSample = 0;
            DacStreamActive = false;
            DacStreamSeq = 0;
            FadeActive = false;
            FadePos = 0;
            Driver_CurLoop = 0;
            memset(&Driver_FmAlgo[0], 0, sizeof(Driver_FmAlgo));
            memset(&TLs[0], 0, sizeof(TLs));
            RhythmTL = 0; //TODO: verify
            AdpcmLevel = 0; //TODO: verify
            Driver_DacEn = 0;
            dcsg_latched_ch = 0;
            for (uint8_t i=0;i<3;i++) dcsg_freq[i] = 0;
            Driver_FirstWait = true;
            memset(&Driver_FmPans[0], 0b11000000, sizeof(Driver_FmPans));
            Driver_Opna_AdpcmConfig = 0b11000000; //todo verify in emu?
            memset(&Driver_Opna_RhythmConfig[0], 0b11000000, sizeof(Driver_Opna_RhythmConfig)); //todo verify in emu
            Driver_Opna_SsgConfig = 0b00111111; //todo verify in emu
            memset(&Driver_Opna_SsgLevel[0], 0, sizeof(Driver_Opna_SsgLevel)); //todo verify in emu
            for (uint8_t i=0;i<4;i++) {
                Driver_DcsgAttenuation[i] = 0b10011111 | (i<<5);
            }
            Driver_UpdateMuting();
            memset((void *)&ChannelMgr_States[0], 0, 6+4);
            DacLastValue = 0;
            DacTouched = false;
            Driver_Slip = 0;
            Driver_BlockOpn2TestReg = false;

            //update status flags
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_FINISHED);
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_START_REQUEST);
            xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING);
            commandeventbits &= ~DRIVER_EVENT_START_REQUEST;
            commandeventbits |= DRIVER_EVENT_RUNNING;
            reset_flag = false;
        } else if (commandeventbits & DRIVER_EVENT_UPDATE_MUTING) {
            ESP_LOGD(TAG, "updating muting upon request");
            Driver_UpdateMuting();
            reset_flag = false;
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_UPDATE_MUTING);
            commandeventbits &= ~DRIVER_EVENT_UPDATE_MUTING;
        } else if (commandeventbits & DRIVER_EVENT_RESET_REQUEST) {
            Driver_ResetChips(false);
            for (uint8_t i=0;i<6+4;i++) {
                ChannelMgr_States[i] = 0;
            }
            Driver_Sample = 0;
            opn2_on_opna_mode = false;
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_FINISHED);
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_RESET_REQUEST);
            xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RESET_ACK);
            commandeventbits &= ~DRIVER_EVENT_RESET_REQUEST;
            commandeventbits |= DRIVER_EVENT_RESET_ACK;
        } else if (commandeventbits & DRIVER_EVENT_STOP_REQUEST) {
            Driver_PauseSample = Driver_Sample;
            Driver_PauseSample_Ds = Driver_Sample_Ds;
            Driver_NoLeds = true;
            if (Driver_DetectedMod == MEGAMOD_NONE) {
                Driver_FmOut(0, 0xb4, Driver_FmPans[0] & 0b00111111);
                Driver_FmOut(0, 0xb5, Driver_FmPans[1] & 0b00111111);
                Driver_FmOut(0, 0xb6, Driver_FmPans[2] & 0b00111111);
                Driver_FmOut(1, 0xb4, Driver_FmPans[3] & 0b00111111);
                Driver_FmOut(1, 0xb5, Driver_FmPans[4] & 0b00111111);
                Driver_FmOut(1, 0xb6, Driver_FmPans[5] & 0b00111111);
            } else if (Driver_DetectedMod == MEGAMOD_OPNA) {
                Driver_FmOutopna(0, 0xb4, Driver_FmPans[0] & 0b00111111);
                Driver_FmOutopna(0, 0xb5, Driver_FmPans[1] & 0b00111111);
                Driver_FmOutopna(0, 0xb6, Driver_FmPans[2] & 0b00111111);
                Driver_FmOutopna(1, 0xb4, Driver_FmPans[3] & 0b00111111);
                Driver_FmOutopna(1, 0xb5, Driver_FmPans[4] & 0b00111111);
                Driver_FmOutopna(1, 0xb6, Driver_FmPans[5] & 0b00111111);
            }
            if (Driver_DetectedMod == MEGAMOD_OPNA) {
                Driver_FmOutopna(0, 0x07, Driver_Opna_SsgConfig | 0b00111111);
            }
            if (Driver_DetectedMod == MEGAMOD_NONE || Driver_DetectedMod == MEGAMOD_OPLLDCSG) {
                for (uint8_t i=0;i<4;i++) {
                    Driver_DcsgOut(0b10011111 | (i<<5));
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
            Driver_Cc = Driver_LastCc = xthal_get_ccount();
            Driver_Sample = Driver_PauseSample;
            Driver_Sample_Ds = Driver_PauseSample_Ds;
            xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING);
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_RESUME_REQUEST);
            commandeventbits &= ~DRIVER_EVENT_RESUME_REQUEST;
            commandeventbits |= DRIVER_EVENT_RUNNING;
        } else if (commandeventbits & DRIVER_EVENT_FASTFORWARD) {
            if (!Driver_Slip) {
                Driver_Cycle += DRIVER_CLOCK_RATE*2;
                Driver_Cycle_Ds += DRIVER_CLOCK_RATE*2;
                Driver_Slip = (1<<0);
                if (DacStreamActive) Driver_Slip |= (1<<1);
                Driver_UpdateMuting();
            }
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_FASTFORWARD);
            commandeventbits &= ~DRIVER_EVENT_FASTFORWARD;
        } else if (commandeventbits & DRIVER_EVENT_RUNNING) {
            uint64_t diff = (Driver_Cc - Driver_LastCc);
            diff *= (1000LL + Driver_SpeedMult);
            diff /= 1000;
            Driver_Cycle += diff;
            Driver_ICycle += diff;
            if (Driver_ICycle >= 400000) {
                uint32_t f = Driver_ICycle/400000;
                Driver_Cycle -= 13*f;
                Driver_ICycle -= 400000*f;
            }
            Driver_LastCc = Driver_Cc;
            Driver_Sample = Driver_Cycle / DRIVER_CYCLES_PER_SAMPLE;
            if (FadeActive) {
                FadePos = Driver_Sample - FadeStart;
                if (Driver_Sample - FadeTimer >= 4410) {
                    FadeTick();
                    FadeTimer = Driver_Sample;
                }
                if (FadePos > 44100*Driver_FadeLength) {
                    ESP_LOGI(TAG, "Fade ended");
                    Stop();
                }
            }
            
            //vgm stuff
            if (Driver_Sample >= Driver_NextSample) { //time to move on to the next sample
                Driver_BusyStart = xthal_get_ccount();
                uint32_t waiting = MegaStream_Used(&Driver_CommandStream);
                if (waiting > 0) { //is any data available?
                    //update half-empty event bit
                    if ((queueeventbits & DRIVER_EVENT_COMMAND_HALF) == 0 && waiting <= DRIVER_QUEUE_SIZE/2) {
                        xEventGroupSetBits(Driver_StreamEvents, DRIVER_EVENT_COMMAND_HALF);
                        queueeventbits |= DRIVER_EVENT_COMMAND_HALF;
                    } else if ((queueeventbits & DRIVER_EVENT_COMMAND_HALF) && waiting > DRIVER_QUEUE_SIZE/2) {
                        xEventGroupClearBits(Driver_StreamEvents, DRIVER_EVENT_COMMAND_HALF);
                        queueeventbits &= ~DRIVER_EVENT_COMMAND_HALF;
                    }

                    uint8_t peeked = MegaStream_Peek(&Driver_CommandStream);
                    uint8_t cmdlen = VgmCommandLength(peeked); //look up the length of this command + attached data
                    if (waiting >= cmdlen) { //if the entire command + data is in the stream
                        //bool ret = Driver_RunCommand(cmdlen);
                        uint8_t cmddata[cmdlen];
                        MegaStream_Recv(&Driver_CommandStream, cmddata, cmdlen);
                        bool ret = mif_exec_next_cmd((mif_cmd_t *)cmddata);
                        if (!ret) {
                            printf("ERR command run fail");
                            fflush(stdout);
                            /*xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_ERROR);
                            commandeventbits |= DRIVER_EVENT_ERROR;*/
                        }
                    } else { //not enough data in stream - underrun
                        xEventGroupSetBits(Driver_StreamEvents, DRIVER_EVENT_COMMAND_UNDERRUN);
                        queueeventbits |= DRIVER_EVENT_COMMAND_UNDERRUN;
                        printf("UNDER data\n");
                        fflush(stdout);
                    }
                } else { //no data at all in stream - underrun
                    xEventGroupSetBits(Driver_StreamEvents, DRIVER_EVENT_COMMAND_UNDERRUN);
                    queueeventbits |= DRIVER_EVENT_COMMAND_UNDERRUN;
                    printf("UNDER none\n");
                    fflush(stdout);
                }
                Driver_CpuUsageVgm += (xthal_get_ccount() - Driver_BusyStart);
            } else {
                //not time for next sample yet
                if (Driver_NextSample - Driver_Sample > 2000 && !DacStreamActive) vTaskDelay(2);
                if (Driver_Slip & (1<<0)) {
                    Driver_Slip &= ~(1<<0);
                    Driver_UpdateMuting();
                }
            }

            //dacstream stuff
            if (DacStreamActive) {
                //todo: gracefully handle the end of a stream
                //can't just go by bytes played because some play modes are based on time
                //decide whether those are worth implementing
                Driver_BusyStart = xthal_get_ccount();
                if (MegaStream_Used((MegaStreamContext_t *)&DacStreamEntries[DacStreamId].Stream)) {
                    Driver_Cycle_Ds += diff;
                    Driver_Sample_Ds = Driver_Cycle_Ds / (DRIVER_CLOCK_RATE/DacStreamSampleRate);
                    if (Driver_Sample_Ds > DacStreamSamplesPlayed) {
                        uint8_t sample;
                        MegaStream_Recv((MegaStreamContext_t *)&DacStreamEntries[DacStreamId].Stream, &sample, 1);
                        if (Driver_DetectedMod == MEGAMOD_NONE) {
                            Driver_FmOut(DacStreamPort, DacStreamCommand, sample);
                        } else if (Driver_DetectedMod == MEGAMOD_OPNA) {
                            Driver_FmOutopna(DacStreamPort, DacStreamCommand, sample);
                        } else if (Driver_DetectedMod == MEGAMOD_2XOPN) {
                            //TODO handle second chip
                            Driver_FmOutopn(DacStreamPort, DacStreamCommand, sample);
                        }
                        DacStreamSamplesPlayed++;
                        if (DacStreamSamplesPlayed == DacStreamDataLength && (DacStreamLengthMode == 0 || DacStreamLengthMode == 1 || DacStreamLengthMode == 3)) {
                            DacStreamActive = false;
                            if (Driver_Slip & (1<<1)) {
                                Driver_Slip &= ~(1<<1);
                                Driver_UpdateMuting();
                            }
                        }
                        DacStreamFailed = false;
                    } else {
                        if (Driver_Slip & (1<<1)) {
                            Driver_Slip &= ~(1<<1);
                            Driver_UpdateMuting();
                        }
                    }
                } else {
                    if (!DacStreamFailed) {
                        ESP_LOGW(TAG, "DacStream sample queue under !! pos %d length %d", DacStreamSamplesPlayed, DacStreamDataLength);
                        DacStreamFailed = true;
                    }
                    //DacStreamActive = false;
                    if (Driver_Slip & (1<<1)) {
                        Driver_Slip &= ~(1<<1);
                        Driver_UpdateMuting();
                    }
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
                    Driver_FmOutopna(1,0x02,pcmoff&0xff);   //start L
                    Driver_FmOutopna(1,0x03,pcmoff>>8);     //start H
                    Driver_FmOutopna(1,0x04,0xff);          //stop L
                    Driver_FmOutopna(1,0x05,0xff);          //stop H
                    Driver_FmOutopna(1,0x0c,0xff);          //limit L
                    Driver_FmOutopna(1,0x0d,0xff);          //limit H
                    Driver_FmOutopna(1,0x00,0x01);          //ADPCM reg0 reset
                    Driver_FmOutopna(1,0x00,0x60);          //ADPCM reg0 REC | MEMDATA
                    //no need to set adpcm reg1 at this point, it should be zeroed from the reset
                    Driver_Opna_PrepareUpload();
                    for (uint32_t i=0;i<pcmsize;i++) {
                        uint8_t b;
                        fread(&b, 1, 1, Driver_Opna_PcmUploadFile);
                        Driver_Opna_UploadByte(b);
                        for (uint8_t j=0;j<=map(i,0,pcmsize,0,6);j++) {
                            ChannelMgr_States[j] |= CHSTATE_PARAM | CHSTATE_KON | CHSTATE_KON_PS;
                        }
                    }
                    for (uint8_t j=0;j<6;j++) {
                        ChannelMgr_States[j] = 0;
                    }
                    Driver_FmOutopna(1,0x00,0x01);          //ADPCM reg0 reset

                    Driver_Opna_PcmUpload = false;
                } else {
                    ESP_LOGE(TAG, "OPNA PCM upload: No file specified!!");
                }
            }

            vTaskDelay(2);
        }
        Driver_CpuPeriod += (xthal_get_ccount() - Driver_Cc);
    }
}
