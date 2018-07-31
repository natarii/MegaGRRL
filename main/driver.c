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

static const char* TAG = "Driver";

#define DRIVER_CLOCK_RATE 240000000 //clock rate of cpu while in playback
#define DRIVER_VGM_SAMPLE_RATE 44100
#define DRIVER_CYCLES_PER_SAMPLE (DRIVER_CLOCK_RATE/DRIVER_VGM_SAMPLE_RATE)

uint8_t Driver_SrBuf[2] = {0xff,0x00}; //buffer to transmit to the shift registers - first byte is the control register, second is data bus

//bits on the control shift reg
#define SR_BIT_PSG_CS   0x01 // PSG /CS
#define SR_BIT_WR       0x04 // /WR
#define SR_BIT_FM_CS    0x20 // FM /CS
#define SR_BIT_A0       0x08 // A0 (only used by FM)
#define SR_BIT_A1       0x10 // A1 (only used by FM)
#define SR_BIT_IC       0x02 // /IC (only used by FM)

QueueHandle_t Driver_CommandQueue; //queue of incoming vgm data
QueueHandle_t Driver_PcmQueue; //queue of attached pcm data (if any)
EventGroupHandle_t Driver_CommandEvents; //driver status flags
EventGroupHandle_t Driver_QueueEvents; //queue status flags

//we abuse the esp32's spi hardware to drive the shift registers
spi_bus_config_t Driver_SpiBusConfig = {
    .miso_io_num=-1,
    .mosi_io_num=PIN_DRIVER_DATA,
    .sclk_io_num=PIN_DRIVER_SHCLK,
    .quadwp_io_num=-1,
    .quadhd_io_num=-1
};
spi_device_interface_config_t Driver_SpiDeviceConfig = {
    .clock_speed_hz=10000000, /*incredibly loud green hill zone music plays*/
    .mode=0,
    .spics_io_num=PIN_DRIVER_SHSTO,
    .queue_size=10
};
spi_device_handle_t Driver_SpiDevice;

//vgm / 2612 pcm stuff
uint32_t Driver_Sample = 0;     //current sample number
uint64_t Driver_Cycle = 0;      //current cycle number
uint32_t Driver_Cc = 0;         //current cycle from the api - just keep it off the stack
uint32_t Driver_LastCc = 0;     //copy of the above var
uint32_t Driver_NextSample = 0; //sample number at which the next command needs to be run

//dacstream specific
uint32_t DacStreamSeq = 0;              //sequence no of the current stream
uint32_t DacStreamLastSeqPlayed = 0;    //last seq successfully played
uint8_t DacStreamId = 0;                //index of the currently playing dacstream. should maybe consider using a pointer to the queue instead of keeping this around
bool DacStreamActive = false;           //actively playing a stream?
uint32_t DacStreamSampleRate = 0;       //stream current sample rate
uint32_t DacStreamSampleTime = 0;       //stream sample timer
uint8_t DacStreamPort = 0;              //chip port to write to
uint8_t DacStreamCommand = 0;           //chip command to use

bool Driver_Setup() {
    //create the queues and event groups
    ESP_LOGI(TAG, "Setting up");
    Driver_CommandQueue = xQueueCreate(DRIVER_QUEUE_SIZE, sizeof(uint8_t));
    if (Driver_CommandQueue == NULL) {
        ESP_LOGE(TAG, "Command queue create failed !!");
        return false;
    }
    Driver_PcmQueue = xQueueCreate(DRIVER_QUEUE_SIZE, sizeof(uint8_t));
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
    txn.tx_buffer = NULL;
    txn.rx_buffer = NULL;
    spi_device_transmit(Driver_SpiDevice, &txn);

    //and now output initial values
    disp_spi_transfer_data(Driver_SpiDevice, (uint8_t*)&Driver_SrBuf, NULL, 2, 0);

    ESP_LOGI(TAG, "Ready");
    return true;
}

void Driver_Sleep(uint32_t us) { //quick and dirty spin sleep
    uint32_t s = xthal_get_ccount();
    uint32_t c = us*(DRIVER_CLOCK_RATE/1000000);
    while (xthal_get_ccount() - s < c);
}

void Driver_Output() { //output data to shift registers
    disp_spi_transfer_data(Driver_SpiDevice, (uint8_t*)&Driver_SrBuf, NULL, 2, 0);
}

void Driver_PsgOut(uint8_t Data) {
    Driver_SrBuf[0] &= ~SR_BIT_PSG_CS;
    Driver_SrBuf[0] &= ~SR_BIT_WR;
    //data bus is reversed for the psg because it made pcb layout easier
    Driver_SrBuf[1] = 0;
    for (uint8_t i=0;i<=7;i++) {
        Driver_SrBuf[1] |= ((Data>>(7-i))&1)<<i;
    }
    Driver_Output();
    Driver_Sleep(20);
    Driver_SrBuf[0] |= SR_BIT_WR;
    Driver_Output();
    Driver_Sleep(20);
    Driver_SrBuf[0] |= SR_BIT_PSG_CS;
    Driver_Output();
    Driver_Sleep(10);
}

void Driver_FmOut(uint8_t Port, uint8_t Register, uint8_t Value) {
    if (Port == 0) {
        Driver_SrBuf[0] &= ~SR_BIT_A1; //clear A1
    } else if (Port == 1) {
        Driver_SrBuf[0] |= SR_BIT_A1; //set A1
    }
    Driver_SrBuf[0] &= ~SR_BIT_A0; //clear A0
    Driver_Output();
    Driver_SrBuf[0] &= ~SR_BIT_FM_CS; // /cs low
    Driver_SrBuf[0] &= ~SR_BIT_WR; // /wr low
    Driver_SrBuf[1] = Register;
    Driver_Output();
    Driver_SrBuf[0] |= SR_BIT_WR; // /wr high
    Driver_Output();
    Driver_SrBuf[0] |= SR_BIT_A0; //set A0
    Driver_SrBuf[0] &= ~SR_BIT_WR; // /wr low
    Driver_SrBuf[1] = Value;
    Driver_Output();
    Driver_SrBuf[0] |= SR_BIT_WR; // /wr high
    Driver_SrBuf[0] |= SR_BIT_FM_CS; // /cs high
    Driver_Output();
    Driver_Sleep(12); //tWWW per datasheet is 83 clocks, at 7.67mhz that's about 11us
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
bool Driver_RunCommand(uint8_t CommandLength) { //run the next command in the queue. command length as a parameter just to avoid looking it up a second time
    uint8_t cmd[CommandLength]; //buffer for command + attached data
    
    //read command + data from the queue
    for (uint8_t i=0;i<CommandLength;i++) {
        xQueueReceive(Driver_CommandQueue, &cmd[i], 0);
    }

    if (cmd[0] == 0x50) { //SN76489
        Driver_PsgOut(cmd[1]);
    } else if (cmd[0] == 0x52) { //YM2612 port 0
        Driver_FmOut(0, cmd[1], cmd[2]);
    } else if (cmd[0] == 0x53) { //YM2612 port 1
        Driver_FmOut(1, cmd[1], cmd[2]);
    } else if (cmd[0] == 0x61) { //16bit wait
        Driver_NextSample += *(uint16_t*)&cmd[1];
    } else if (cmd[0] == 0x62) { //60Hz wait
        Driver_NextSample += 735;
    } else if (cmd[0] == 0x63) { //50Hz wait
        Driver_NextSample += 882;
    } else if ((cmd[0] & 0xf0) == 0x70) { //4bit wait
        Driver_NextSample += (cmd[0] & 0x0f) + 1;
    } else if ((cmd[0] & 0xf0) == 0x80) { //YM2612 DAC + wait
        uint8_t sample;
        //TODO check if queue is empty
        xQueueReceive(Driver_PcmQueue, &sample, 0);
        Driver_FmOut(0, 0x2a, sample);
        Driver_NextSample += cmd[0] & 0x0f;
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
            ESP_LOGD(TAG, "playing %d q size %d", DacStreamSeq, uxQueueMessagesWaiting(DacStreamEntries[DacStreamId].Queue));
            DacStreamLastSeqPlayed = DacStreamSeq;
            DacStreamActive = true;
        }
    } else if (cmd[0] == 0x94) { //dac stream stop
        DacStreamActive = false;
    } else if (cmd[0] == 0x92) { //set sample rate
        if (DacStreamActive) memcpy(&DacStreamSampleRate, &cmd[1], sizeof(uint32_t));
    } else {
        return false;
    }
    return true;
}
void Driver_Main() {
    //driver task. never pet watchdog or come up for air at all - nothing else is running on CPU1.
    ESP_LOGI(TAG, "Task start");
    while (1) {
        Driver_Cc = xthal_get_ccount();
        uint8_t commandeventbits = xEventGroupGetBits(Driver_CommandEvents);
        uint8_t queueeventbits = xEventGroupGetBits(Driver_QueueEvents);
        if (commandeventbits & DRIVER_EVENT_START_REQUEST) {

            //HACK BAD RESET
            Driver_SrBuf[0] ^= SR_BIT_IC;
            Driver_Output();
            Driver_Sleep(100000);
            Driver_SrBuf[0] |= SR_BIT_IC;
            Driver_Output();
            Driver_Sleep(100000);
            
            //reset all internal vars
            Driver_Sample = 0;
            Driver_Cycle = 0;
            Driver_LastCc = Driver_Cc = xthal_get_ccount();
            Driver_NextSample = 0;

            //update status flags
            xEventGroupClearBits(Driver_CommandEvents, DRIVER_EVENT_START_REQUEST);
            xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_RUNNING);
            commandeventbits &= ~DRIVER_EVENT_START_REQUEST;
            commandeventbits |= DRIVER_EVENT_RUNNING;
        }
        if (commandeventbits & DRIVER_EVENT_RUNNING) {
            Driver_Cycle += (Driver_Cc - Driver_LastCc);
            Driver_LastCc = Driver_Cc;
            Driver_Sample = Driver_Cycle / DRIVER_CYCLES_PER_SAMPLE;
            
            //vgm stuff
            if (Driver_Sample >= Driver_NextSample) { //time to move on to the next sample
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
                            printf("ERR");
                            fflush(stdout);
                            xEventGroupSetBits(Driver_CommandEvents, DRIVER_EVENT_ERROR);
                            commandeventbits |= DRIVER_EVENT_ERROR;
                        }
                    } else { //not enough data in queue - underrun
                        xEventGroupSetBits(Driver_QueueEvents, DRIVER_EVENT_COMMAND_UNDERRUN);
                        queueeventbits |= DRIVER_EVENT_COMMAND_UNDERRUN;
                        printf("UNDER\n");
                        fflush(stdout);
                    }
                } else { //no data at all in queue - underrun
                    xEventGroupSetBits(Driver_QueueEvents, DRIVER_EVENT_COMMAND_UNDERRUN);
                    queueeventbits |= DRIVER_EVENT_COMMAND_UNDERRUN;
                    printf("UNDER\n");
                    fflush(stdout);
                }
            } else {
                //not time for next sample yet
            }

            //dacstream stuff
            if (DacStreamActive) {
                //todo: gracefully handle the end of a stream
                //can't just go by bytes played because some play modes are based on time
                //decide whether those are worth implementing
                if (uxQueueMessagesWaiting(DacStreamEntries[DacStreamId].Queue)) {
                    if (xthal_get_ccount() - DacStreamSampleTime >= (DRIVER_CLOCK_RATE/DacStreamSampleRate)) {
                        uint8_t sample;
                        xQueueReceive(DacStreamEntries[DacStreamId].Queue, &sample, 0);
                        Driver_FmOut(DacStreamPort, DacStreamCommand, sample);
                        DacStreamSampleTime += (DRIVER_CLOCK_RATE/DacStreamSampleRate);
                    }
                } else {
                    //ESP_LOGW(TAG, "DacStream under, stopping !!");
                    //DacStreamActive = false;
                }
            }
        } else {
            //not running
        }
    }
}