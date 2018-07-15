#ifndef AGR_IOEXP_H
#define AGR_IOEXP_H

#define I2C_ADDR_IOEXP 0b0100000

bool IoExp_Setup();
void IoExp_Main();
bool IoExp_WriteLed(uint8_t LedNo, bool LedStatus);
bool IoExp_WriteLeds(bool FlashlightStatus, bool Led1Status, bool Led2Status);
bool IoExp_WriteLedsBin(uint8_t ThreeBits);

#endif