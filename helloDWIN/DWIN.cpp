#include "DWIN.h"

#define MIN_ASCII 32
#define MAX_ASCII 255

#define FRAME_HEADER_POSTION1 0
#define FRAME_HEADER_POSTION2 1
#define LENGTH_SEND_CMD_POSITION 2
#define CMD_POSITION 3
#define HIGH_VP_POSITION 4
#define LOW_VP_POSITION 5
#define HIGH_KEY_POSITION 7
#define LOW_KEY_POSITION 8
#define MAX_RESPONE_LENGTH 260
#define _GET_VP_ADDRESS(arr, sizeArray) ((sizeArray <= LOW_VP_POSITION) ? 0 : ((((uint16_t)(arr)[HIGH_VP_POSITION]) << 8) | (arr)[LOW_VP_POSITION]))
#define _NOT(condition) !(condition)

#define TIME_OUT_RECEIVE 1000

void printfArrDebug(uint8_t *arr, uint16_t size) {
  for (uint16_t i = 0; i < size; i++) {
    Serial.printf("%x ", arr[i]);
  }
  Serial.println();
}

TaskHandle_t DWIN::xTaskDWINHandle;

DWIN::DWIN(HardwareSerial &port, uint8_t receivePin, uint8_t transmitPin, long baud, uint16_t sizeLeaseQueue)
  : _dwinSerial((HardwareSerial *)&port),
    listenerCallback(NULL),
    _baudrate(baud),
    _rxPin(receivePin),
    _txPin(transmitPin),
    u8CountFalse(0) {
}

void DWIN::begin(uint32_t u32StackDepthReceive, BaseType_t xNPriority) {
  _dwinSerial->begin(_baudrate, SERIAL_8N1, _rxPin, _txPin);
  delay(10);
  xQueueCommandReadWrite = xQueueCreate(1, sizeof(PendingRequest_t));
  if (xQueueCommandReadWrite == NULL) {
    Serial.printf("không khởi tạo được xQueueCommandReadWrite => reset\n");
    delay(1000);
    esp_restart();
  }
  delay(10);
  xQueueResponeDataEvent = xQueueCreate(1, MAX_RESPONE_LENGTH);
  if (xQueueCommandReadWrite == NULL) {
    Serial.printf("không khởi tạo được xQueueResponeDataEvent => reset\n");
    delay(1000);
    esp_restart();
  }
  delay(10);
  xMutexPendingRequest = xSemaphoreCreateMutex();
  if (xQueueCommandReadWrite == NULL) {
    Serial.printf("không khởi tạo được xMutexPendingRequest => reset\n");
    delay(1000);
    esp_restart();
  }
  delay(10);
  xTaskCreatePinnedToCore(xTaskReceiveUartEvent, "DWINEvent", u32StackDepthReceive, (void *)this, xNPriority, &xTaskDWINHandle, tskNO_AFFINITY);
  delay(10);
  _dwinSerial->onReceive(vTriggerTaskReceiveFromUartEvent, true);
}

void DWIN::setupTouchCallBack(QueueHandle_t *pxTouchQueue, uint16_t sizeOfQueue) {
  (*pxTouchQueue) = xQueueCreate(sizeOfQueue, sizeof(TouchFrame_t));
  xQueueTouch = (*pxTouchQueue);
  if (xQueueTouch == NULL) {
    ESP_LOGE(__FILE__, "%s can't create Queue => esp restart", __func__);
    esp_restart();
  }
}
void DWIN::setupTouchCallBack(hmiListener callBackTouch) {
  xQueueTouch = xQueueCreate(5, sizeof(TouchFrame_t));
  if (_NOT(callBackTouch)) {
    ESP_LOGE(__FILE__, "%s INVALID PRAMETER", __func__);
    return;
  }
  listenerCallback = callBackTouch;
}

void DWIN::echoEnabled(bool enabled) {
  _echo = enabled;
}
void DWIN::crcEnabled(bool enabled) {
  _crc = enabled;
}

/**
 *@brief send the dwinSendArray and get the respone
 *
 * @param dwinSendArray data send to DWIN without Frame header and Length. example 5AA5 06 (83 00 0F 01 14 10) <---
 * @param arraySize size of dwinSendArray
 * @param pu8OutData pu8BufferReceiver get data respone from DWIN display. input NULL if you dont need the respone
 * @param u16OutDataSize size of pu8OutData. it should be greater than or equal to expect respone data from DWIN display
 *                       Maxvalue is MAX_RESPONE_LENGTH
 * @return esp_err_t ESP_OK: send dwinSendArray and get respone in timeout
 */
esp_err_t DWIN::sendArray(const uint8_t *dwinSendArray, uint8_t arraySize, uint8_t *pu8OutData, uint16_t u16OutDataSize, uint16_t u16TimeOutInSecond) {
  if (dwinSendArray == NULL || arraySize < 3) {
    ESP_LOGE(__func__, "loi tham so");
    return 0;
  }

  //*chuẩn bị dữ liệu
  uint8_t dataLen = arraySize + 3;
  if (_crc) {
    dataLen += 2;
  }
  uint8_t sendBuffer[dataLen] = { CMD_HEAD1, CMD_HEAD2, dataLen - 3 };
  uint16_t u16SizeOfSendBuffer = sizeof(sendBuffer) / sizeof(sendBuffer[0]);
  memcpy(sendBuffer + 3, dwinSendArray, arraySize);
  if (_crc) {
    uint16_t crc = u16CalculateCRCModbus(sendBuffer + 3, u16SizeOfSendBuffer - 5);
    sendBuffer[u16SizeOfSendBuffer - 2] = crc & 0xFF;
    sendBuffer[u16SizeOfSendBuffer - 1] = (crc >> 8) & 0xFF;
  }

  //* đẩy vào queue chờ
  uint16_t VPaddress = (((uint16_t)sendBuffer[HIGH_VP_POSITION]) << 8) | sendBuffer[LOW_VP_POSITION];
  PendingRequest_t pending = {
    .u8cmd = sendBuffer[CMD_POSITION],
    .u16VPaddress = VPaddress,
    .xQueueGetResponeData = xQueueResponeDataEvent,
  };
  if (xSemaphoreTake(xMutexPendingRequest, portMAX_DELAY)) {
    if (xQueueCommandReadWrite) {
      xQueueSend(xQueueCommandReadWrite, &pending, portMAX_DELAY);
    }
  }

  //* gửi uart
  _dwinSerial->write(sendBuffer, sizeof(sendBuffer));

  //*chờ dữ liệu về
  bool ret = ESP_FAIL;
  uint16_t u16ExtimateSizeOfRespone = u6CalcuSizeOfResponeBuffer(dwinSendArray, arraySize);
  uint8_t arr[u16ExtimateSizeOfRespone] = {};
  if (u8CountFalse > 20) {  // nếu lỗi quá nhiều => hư màn hình không chờ màn hình phản hồi nữa tốn thời gian
    u16TimeOutInSecond = 0;
    u8CountFalse = 20;
  }
  ret = xQueueReceive(xQueueResponeDataEvent, arr, pdMS_TO_TICKS(u16TimeOutInSecond));
  if (ret == pdPASS) {
    if (pu8OutData && u16OutDataSize >= u16ExtimateSizeOfRespone) {
      memcpy(pu8OutData, arr, u16ExtimateSizeOfRespone);
    }
    ret = ESP_OK;
    u8CountFalse = 0;
  } else {
    ret = ESP_FAIL;
    u8CountFalse++;
    Serial.printf("khong nhan duoc phan hoi %x, %x\n", VPaddress, sendBuffer[CMD_POSITION]);
  }

  //* trả queue
  xQueueReceive(xQueueCommandReadWrite, &pending, 0);
  xSemaphoreGive(xMutexPendingRequest);
  return ret;
}
esp_err_t DWIN::setVPWord(uint16_t address, uint16_t data, uint8_t *pu8OutData, uint16_t u16OutDataSize, uint16_t u16TimeOutInSecond) {
  // 0x5A, 0xA5, 0x05, 0x82, hiVPaddress, loVPaddress, hiData, loData
  uint8_t sendBuffer[] = { CMD_WRITE, (uint8_t)((address >> 8) & 0xFF), (uint8_t)((address)&0xFF), (uint8_t)((data >> 8) & 0xFF), (uint8_t)((data)&0xFF) };

  return sendArray(sendBuffer, sizeof(sendBuffer), pu8OutData, u16OutDataSize, u16TimeOutInSecond);
}
esp_err_t DWIN::readVPWord(uint16_t address, uint8_t numWords, uint8_t *pu8OutData, uint16_t u16OutDataSize, uint16_t u16TimeOutInSecond) {
  uint8_t sendBuffer[] = { CMD_READ, (uint8_t)((address >> 8) & 0xFF), (uint8_t)((address)&0xFF), numWords };

  return sendArray(sendBuffer, sizeof(sendBuffer), pu8OutData, u16OutDataSize, u16TimeOutInSecond);
}
esp_err_t DWIN::setVPByte(uint16_t address, uint8_t data, uint8_t *pu8OutData, uint16_t u16OutDataSize, uint16_t u16TimeOutInSecond) {
  return setVPWord(address, data, pu8OutData, u16OutDataSize, u16TimeOutInSecond);
}

double DWIN::getHWVersion() {  //  HEX(5A A5 04 83 00 0F 01)
  const uint8_t sendBuffer[] = { CMD_READ, 0x00, 0x0F, 0x01 };
  uint16_t u16SizeOfResponeBuffer = u6CalcuSizeOfResponeBuffer(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
  uint8_t pu8Data[u16SizeOfResponeBuffer] = {};

  sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]), pu8Data, u16SizeOfResponeBuffer);

  return pu8Data[8];
}
double DWIN::getGUISoftVersion() {
  //  HEX(5A A5 04 83 00 0F 01)
  const uint8_t sendBuffer[] = { CMD_READ, 0x00, 0x0F, 0x01 };
  uint16_t u16SizeOfResponeBuffer = u6CalcuSizeOfResponeBuffer(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
  uint8_t pu8Data[u16SizeOfResponeBuffer] = {};

  sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]), pu8Data, u16SizeOfResponeBuffer);

  return pu8Data[7];
}
esp_err_t DWIN::restartHMI() {
  // HEX(5A A5 07 82 00 04 55 aa 5a a5 )
  const uint8_t sendBuffer[] = { CMD_WRITE, 0x00, 0x04, 0x55, 0xaa, CMD_HEAD1, CMD_HEAD2 };
  uint16_t u16SizeOfResponeBuffer = u6CalcuSizeOfResponeBuffer(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
  uint8_t pu8Data[u16SizeOfResponeBuffer] = {};

  return (sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]), pu8Data, u16SizeOfResponeBuffer);
}

esp_err_t DWIN::setPage(uint8_t pageID) {
  uint8_t sendBuffer[] = { CMD_WRITE, 0x00, 0x84, 0x5A, 0x01, 0x00, pageID };

  return sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
}
uint8_t DWIN::getPage() {
  const uint8_t sendBuffer[] = { CMD_READ, 0x00, 0x14, 0x01 };
  uint16_t u16SizeOfResponeBuffer = u6CalcuSizeOfResponeBuffer(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
  uint8_t pu8Data[u16SizeOfResponeBuffer] = {};

  sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]), pu8Data, u16SizeOfResponeBuffer);
  // 5A A5 06 83 00 14 01 00 (07) <= return
  return pu8Data[8];
}

esp_err_t DWIN::setText(uint16_t VPaddress, String textData) {
  return setText(VPaddress, textData, textData.length());
}

esp_err_t DWIN::setText(uint16_t VPaddress, String textData, uint16_t TextLength) {
  const uint8_t ffEnding[2] = { 0xFF, 0xFF };
  uint8_t startCMD[] = { CMD_WRITE, (uint8_t)((VPaddress >> 8) & 0xFF), (uint8_t)((VPaddress)&0xFF) };

  uint8_t dataCMD[TextLength] = {};
  memcpy(dataCMD, textData.c_str(), TextLength);

  uint8_t sendBuffer[5 + TextLength] = {};
  memcpy(sendBuffer, startCMD, sizeof(startCMD));
  memcpy(sendBuffer + 3, dataCMD, sizeof(dataCMD));
  memcpy(sendBuffer + (3 + sizeof(dataCMD)), ffEnding, 2);

  return sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
}
String DWIN::getText(uint16_t VPaddress, uint8_t length) {
  uint8_t numWords = length / 2 + 1;
  uint8_t sendBuffer[] = { CMD_READ, (uint8_t)((VPaddress >> 8) & 0xFF), (uint8_t)((VPaddress)&0xFF), numWords };
  uint16_t u16SizeOfResponeBuffer = u6CalcuSizeOfResponeBuffer(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
  uint8_t pu8Data[u16SizeOfResponeBuffer] = {};

  sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]), pu8Data, u16SizeOfResponeBuffer);

  String responeText = "";
  // 5a a5 10 83 80 0 5 41 42 ff ff 0 0 0 0 0 0 fc f6
  // 5a a5 3a 83 b8 0 1a ff ff

  uint16_t i = 7;

  while (_NOT(pu8Data[i] == MAX_ASCII && pu8Data[i + 1] == MAX_ASCII) && (i < (pu8Data[LENGTH_SEND_CMD_POSITION] + 3))) {
    if (isascii(pu8Data[i])) {
      responeText += (char)pu8Data[i];
    }
    i++;
  }

  return responeText;
}
esp_err_t DWIN::setTextColor(uint16_t spAddress, uint16_t spOffset, uint16_t color, uint8_t *pu8OutData, uint16_t u16OutDataSize, uint16_t u16TimeOutInSecond) {
  // 0x5A, 0xA5, 0x05, hi spAddress, lo spAddress, hi color, lo color
  spAddress = (spAddress + spOffset);
  uint8_t sendBuffer[] = { CMD_WRITE, (uint8_t)((spAddress >> 8) & 0xFF), (uint8_t)((spAddress)&0xFF), (uint8_t)((color >> 8) & 0xFF), (uint8_t)((color)&0xFF) };

  return sendArray(sendBuffer, sizeof(sendBuffer), pu8OutData, u16OutDataSize, u16TimeOutInSecond);
}

esp_err_t DWIN::setBrightness(uint8_t brightness) {
  uint8_t sendBuffer[] = { CMD_WRITE, 0x00, 0x82, brightness };

  return sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
}
uint8_t DWIN::getBrightness() {
  const uint8_t sendBuffer[] = { CMD_READ, 0x00, 0x31, 0x01 };
  uint16_t u16SizeOfResponeBuffer = u6CalcuSizeOfResponeBuffer(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
  uint8_t pu8Data[u16SizeOfResponeBuffer] = {};

  sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]), pu8Data, u16SizeOfResponeBuffer);

  return pu8Data[8];
}

esp_err_t DWIN::playSound(uint8_t soundID) {
  // 5A A5 07 82 00 A0 soundID 01 40 00
  uint8_t sendBuffer[] = { CMD_WRITE, 0x00, 0xA0, soundID, 0x01, 0x40, 0x00 };

  return sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
}
esp_err_t DWIN::beepHMI(int time) {
  // 1s = 1000ms/8ms = 125 = 0x7D
  // 0x5A, 0xA5, 0x05, 0x82, 0x00, 0xA0, 0x00, 0x7D
  if (time < 8) {
    time = 8;
  } else {
    time = time / 8;
  }
  uint8_t sendBuffer[] = { CMD_WRITE, 0x00, 0xA0, (uint8_t)((time >> 8) & 0xFF), (uint8_t)((time)&0xFF) };

  return sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
}

// set the hardware RTC The first two digits of the year are automatically added
esp_err_t DWIN::setRTC(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
  // 5A A5 0B 82 00 9C 5A A5 year month day hour minute second
  uint8_t sendBuffer[] = { CMD_WRITE, 0x00, 0x9C, 0x5A, 0xA5, year, month, day, hour, minute, second };

  return sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
}
// upHMI_DATE the software RTC The first two digits of the year are automatically added
esp_err_t DWIN::setRTCSOFT(uint8_t year, uint8_t month, uint8_t day, uint8_t weekday, uint8_t hour, uint8_t minute, uint8_t second) {
  // 5A A5 0B 82 0010 year month day weekday(0-6 0=Sunday) hour minute second 00
  uint8_t sendBuffer[] = { CMD_WRITE, 0x00, 0x10, year, month, day, weekday, hour, minute, second, 0x00 };

  return sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
}

esp_err_t DWIN::setGraphYCentral(uint16_t spAddr, int value) {
  return setVPWord(spAddr + 0x0005, value);
}
esp_err_t DWIN::setGraphVDCentral(uint16_t spAddr, int value) {
  return setVPWord(spAddr + 0x0006, value);
}
esp_err_t DWIN::setGraphColor(uint16_t spAddr, int value) {
  return setVPWord(spAddr + 0x0007, value);
}
esp_err_t DWIN::setGraphRightToLeft(uint16_t spAddr) {
  return setVPWord(spAddr, 0);
}
esp_err_t DWIN::setGraphLeftToRight(uint16_t spAddr) {
  return setVPWord(spAddr, 1);
}
esp_err_t DWIN::setGraphMulY(uint16_t spAddr, int value) {
  return setVPWord(spAddr + 0x0008, value);
}

esp_err_t DWIN::sendGraphValue(uint16_t channel, const uint16_t *values, uint8_t valueCount) {
  // Độ dài mảng cần gửi (bao gồm header và data)
  size_t dataSize = 9 + valueCount * 2;

  uint8_t sendBuffer[dataSize] = { CMD_WRITE, 0x03, 0x10, 0x5A, 0xA5, 0x01, 0x00, (uint8_t)(channel & 0xFF), valueCount };
  for (int i = 0; i < valueCount; ++i) {
    sendBuffer[9 + i * 2] = static_cast<uint8_t>((values[i] >> 8) & 0xFF);  // Byte cao
    sendBuffer[10 + i * 2] = static_cast<uint8_t>(values[i] & 0xFF);        // Byte thấp
  }

  return sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
}
esp_err_t DWIN::sendGraphValue(uint8_t channel, uint16_t value) {
  uint8_t sendBuffer[] = { 0x82, 0x03, 0x10, 0x5A, 0xA5, 0x01, 0x00, channel, 0x01, (uint8_t)((value >> 8) & 0xFF), (uint8_t)((value)&0xFF) };

  return sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
}
esp_err_t DWIN::resetGraph(uint8_t channel) {
  uint8_t data_reset = 0x01;  // data_reset for channel 0
  data_reset += channel * 2;
  uint8_t sendBuffer[] = { CMD_WRITE, 0x03, data_reset, 0x00, 0x00 };

  return sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
}

esp_err_t DWIN::xDrawCircle(
  uint16_t u16VPaddress,
  uint16_t CenterX00,
  uint16_t CenterY00,
  uint16_t u16Radius,
  const uint16_t u16Color) {

  const uint16_t u16NumberOfCircle = 0x0001;
  uint16_t u16GraphicDrawCMD = 0x0005;
  uint16_t u16TotalByteCircleData = 8;
  if (u16Radius == 0) {
    u16GraphicDrawCMD = 0x0008;
    u16TotalByteCircleData = 6;  // lệnh fill không cần bán kính
  }
  // u16CommandSize là kích thước của payload DWIN, tức là từ 0x82 đến 0x00 (kết thúc FF 00)
  // 1 (0x82) + 2 (VP) + 2 (CMD: 0x0005) + 2 (Quantity) + u16TotalByteCircleData + 2 (0xFF 0x00)
  uint16_t u16CommandSize = u16TotalByteCircleData + 1 + 2 + 2 + 2 + 2;
  uint8_t sendBuffer[u16CommandSize] = {};

  uint8_t u8IndexCircle = 0;
  sendBuffer[u8IndexCircle++] = CMD_WRITE;                                   // write
  sendBuffer[u8IndexCircle++] = (uint8_t)((u16VPaddress >> 8) & 0xFF);       // VP High byte
  sendBuffer[u8IndexCircle++] = (uint8_t)((u16VPaddress)&0xFF);              // VP Low byte
  sendBuffer[u8IndexCircle++] = (uint8_t)((u16GraphicDrawCMD >> 8) & 0xFF);  // CMD High byte (for 0x0005)
  sendBuffer[u8IndexCircle++] = (uint8_t)((u16GraphicDrawCMD)&0xFF);         // CMD Low byte (for 0x0005 - draw circle)
  sendBuffer[u8IndexCircle++] = (uint8_t)((u16NumberOfCircle >> 8) & 0xFF);  // Quantity High byte (usually 0x00)
  sendBuffer[u8IndexCircle++] = (uint8_t)(u16NumberOfCircle & 0xFF);         // Quantity Low byte

  // Tọa độ X (Big-Endian)
  sendBuffer[u8IndexCircle++] = (uint8_t)((CenterX00 >> 8) & 0xFF);
  sendBuffer[u8IndexCircle++] = (uint8_t)(CenterX00 & 0xFF);

  // Tọa độ Y (Big-Endian)
  sendBuffer[u8IndexCircle++] = (uint8_t)((CenterY00 >> 8) & 0xFF);
  sendBuffer[u8IndexCircle++] = (uint8_t)(CenterY00 & 0xFF);

  // Bán kính (Big-Endian: 0x00, u16Radius)
  // CẢNH BÁO: DWIN spec quy định bán kính là 1 byte cho lệnh 0x0005.
  // Việc gửi 2 byte (0x00, u16Radius) có thể gây ra lỗi hoặc hành vi không mong muốn.
  if (u16Radius) {
    sendBuffer[u8IndexCircle++] = (uint8_t)((u16Radius >> 8) & 0xFF);  // High byte (sẽ là 0x00)
    sendBuffer[u8IndexCircle++] = (uint8_t)(u16Radius & 0xFF);         // Low byte (giá trị bán kính thực)
  }

  // Màu sắc (Big-Endian)
  sendBuffer[u8IndexCircle++] = (uint8_t)((u16Color >> 8) & 0xFF);
  sendBuffer[u8IndexCircle++] = (uint8_t)(u16Color & 0xFF);

  sendBuffer[u8IndexCircle++] = 0xFF;  // end frame (High byte)
  sendBuffer[u8IndexCircle++] = 0x00;  // end frame (Low byte)

  return sendArray(sendBuffer, sizeof(sendBuffer) / sizeof(sendBuffer[0]));
}

esp_err_t DWIN::xCalcuCenterMatrixCircle(
  uint8_t *pu8CmdOut,
  uint16_t u16SizeCmdOut,
  uint16_t u16VPaddress,
  uint16_t CenterX00,
  uint16_t CenterY00,
  uint16_t u16Radius,
  int16_t i16DisX,
  int16_t i16DisY,
  const uint16_t *pu16Color,
  uint16_t u16Row,
  uint16_t u16Col) {

  if (NULL == pu8CmdOut) {
    ESP_LOGE("", "pu8CmdOut is NULL");
    return ESP_ERR_INVALID_ARG;
  }

  uint16_t u16NumberOfCircle = u16Row * u16Col;
  uint16_t u16BytePerCircleData = 8;  // X(2B), Y(2B), R(2B), Color(2B) = 8 bytes per circle data block
  if (u16Radius == 0) {
    u16BytePerCircleData -= 2;  // lệnh fill không cần bán kính
  }
  uint16_t u16TotalByteCircleData = u16NumberOfCircle * u16BytePerCircleData;
  // u16CommandSize là kích thước của payload DWIN, tức là từ 0x82 đến 0x00 (kết thúc FF 00)
  // 1 (0x82) + 2 (VP) + 2 (CMD: 0x0005) + 2 (Quantity) + u16TotalByteCircleData + 2 (0xFF 0x00)
  uint16_t u16CommandSize = u16TotalByteCircleData + 1 + 2 + 2 + 2 + 2;

  if (u16SizeCmdOut < u16CommandSize) {
    ESP_LOGE("", "u16SizeCmdOut < u16CommandSize");
    return ESP_ERR_INVALID_ARG;
  }

  uint16_t u8IndexCircle = 0;

  // Điền dữ liệu cho từng hình tròn
  for (uint16_t r = 0; r < u16Row; ++r) {
    for (uint16_t c = 0; c < u16Col; ++c) {
      uint16_t index_in_1d_color_array = r * u16Col + c;  // Tính chỉ mục trong mảng màu 1D
      uint16_t current_x = CenterX00 + (c * i16DisX);
      uint16_t current_y = CenterY00 + (r * i16DisY);
      uint16_t current_color = pu16Color ? pu16Color[index_in_1d_color_array] : 0xF800;  // Lấy màu từ mảng 1D pu16Color

      // Tọa độ X (Big-Endian)
      pu8CmdOut[u8IndexCircle++] = (uint8_t)((current_x >> 8) & 0xFF);
      pu8CmdOut[u8IndexCircle++] = (uint8_t)(current_x & 0xFF);

      // Tọa độ Y (Big-Endian)
      pu8CmdOut[u8IndexCircle++] = (uint8_t)((current_y >> 8) & 0xFF);
      pu8CmdOut[u8IndexCircle++] = (uint8_t)(current_y & 0xFF);

      // Bán kính (Big-Endian: 0x00, u16Radius)
      // CẢNH BÁO: DWIN spec quy định bán kính là 1 byte cho lệnh 0x0005.
      // Việc gửi 2 byte (0x00, u16Radius) có thể gây ra lỗi hoặc hành vi không mong muốn.
      if (u16Radius) {
        pu8CmdOut[u8IndexCircle++] = (uint8_t)((u16Radius >> 8) & 0xFF);  // High byte (sẽ là 0x00)
        pu8CmdOut[u8IndexCircle++] = (uint8_t)(u16Radius & 0xFF);         // Low byte (giá trị bán kính thực)
      }

      // Màu sắc (Big-Endian)
      pu8CmdOut[u8IndexCircle++] = (uint8_t)((current_color >> 8) & 0xFF);
      pu8CmdOut[u8IndexCircle++] = (uint8_t)(current_color & 0xFF);
    }
  }
  return ESP_OK;
}

esp_err_t DWIN::xDrawMatrixCircle(
  uint16_t u16VPaddress,
  uint16_t CenterX00,
  uint16_t CenterY00,
  uint16_t u16Radius,
  int16_t i16DisX,
  int16_t i16DisY,
  const uint16_t *pu16Color,
  uint16_t u16Row,
  uint16_t u16Col) {

  if (u16Row == 0 || u16Col == 0) {
    ESP_LOGE("", "u16Row == 0 || u16Col == 0");
    return ESP_ERR_INVALID_ARG;
  }

  if (NULL == pu16Color) {
    ESP_LOGW("", "NULL == pu16Color");
  }

  uint16_t u16NumberOfCircle = u16Row * u16Col;
  if (u16NumberOfCircle > 242) {
    ESP_LOGE("", "u16NumberOfCircle > 242");
    return ESP_ERR_INVALID_ARG;
  }

  uint16_t u16BytePerCircleData = 8;  // X(2B), Y(2B), R(2B), Color(2B) = 8 bytes per circle data block
  uint16_t u16GraphicDrawCMD = 0x0005;
  if (u16Radius == 0) {
    u16BytePerCircleData = 6;  // lệnh fill không cần bán kính
    u16GraphicDrawCMD = 0x0008;
  }
  uint16_t u16TotalByteCircleData = u16NumberOfCircle * u16BytePerCircleData;

  // u16CommandSize là kích thước của payload DWIN, tức là từ 0x82 đến 0x00 (kết thúc FF 00)
  // 1 (0x82) + 2 (VP) + 2 (CMD: 0x0005) + 2 (Quantity) + u16TotalByteCircleData + 2 (0xFF 0x00)
  uint16_t u16CommandSize = u16TotalByteCircleData + 1 + 2 + 2 + 2 + 2;

  // Sử dụng VLA (Variable Length Array) - cần C99 hoặc C11.
  // Kiểm tra kích thước tổng thể để đảm bảo không bị tràn stack nếu VLA quá lớn.
  // Kích thước tối đa cho ESP32 VLA cần kiểm tra cụ thể trong tài liệu ESP-IDF nếu quá lớn.
  uint8_t u8CircleSendData[u16CommandSize] = {};
  uint16_t u8IndexCircle = 0;

  // Điền các byte đầu tiên của payload DWIN
  u8CircleSendData[u8IndexCircle++] = CMD_WRITE;                                   // write
  u8CircleSendData[u8IndexCircle++] = (uint8_t)((u16VPaddress >> 8) & 0xFF);       // VP High byte
  u8CircleSendData[u8IndexCircle++] = (uint8_t)((u16VPaddress)&0xFF);              // VP Low byte
  u8CircleSendData[u8IndexCircle++] = (uint8_t)((u16GraphicDrawCMD >> 8) & 0xFF);  // CMD High byte (for 0x0005)
  u8CircleSendData[u8IndexCircle++] = (uint8_t)((u16GraphicDrawCMD)&0xFF);         // CMD Low byte (for 0x0005 - draw circle)
  u8CircleSendData[u8IndexCircle++] = (uint8_t)((u16NumberOfCircle >> 8) & 0xFF);  // Quantity High byte (usually 0x00)
  u8CircleSendData[u8IndexCircle++] = (uint8_t)(u16NumberOfCircle & 0xFF);         // Quantity Low byte

  xCalcuCenterMatrixCircle(u8CircleSendData + u8IndexCircle, u16CommandSize, u16VPaddress, CenterX00, CenterY00, u16Radius, i16DisX, i16DisY, pu16Color, u16Row, u16Col);
  // Điền 0xFF 0x00 kết thúc payload
  u8CircleSendData[u16CommandSize - 2] = 0xFF;  // end frame (High byte)
  u8CircleSendData[u16CommandSize - 1] = 0x00;  // end frame (Low byte)

  return sendArray(u8CircleSendData, u16CommandSize, NULL, 0, 2000);
}

// --- Hàm xCalcuPointMatrixRectangle mới ---
// Hàm này chỉ tính toán và điền dữ liệu tọa độ và màu cho từng hình chữ nhật.
// Nó không điền header DWIN hay các trường lệnh khác.
esp_err_t DWIN::xCalcuPointMatrixRectangle(
  uint8_t *pu8CmdOut,      // Buffer để điền dữ liệu (tọa độ và màu)
  uint16_t u16SizeCmdOut,  // Kích thước tối đa của phần buffer này
  uint16_t CenterX00,
  uint16_t CenterY00,
  uint16_t u16Length,  // Chiều dài hình chữ nhật
  uint16_t u16Width,   // Chiều rộng hình chữ nhật
  int16_t i16DisX,
  int16_t i16DisY,
  const uint16_t *pu16Color,  // Con trỏ 1D tới mảng màu
  uint16_t u16Row,
  uint16_t u16Col) {

  if (NULL == pu8CmdOut) {
    ESP_LOGE("xCalcuPointMatrixRectangle", "pu8CmdOut is NULL");
    return ESP_ERR_INVALID_ARG;
  }
  if (u16Row == 0 || u16Col == 0) {
    ESP_LOGE("xCalcuPointMatrixRectangle", "u16Row == 0 || u16Col == 0");
    return ESP_ERR_INVALID_ARG;
  }

  uint16_t u16NumberOfRectangle = u16Row * u16Col;
  const uint16_t u16BytePerRectangleData = 10;  // Xs(2B), Ys(2B), Xe(2B), Ye(2B), Color(2B) = 10 bytes
  uint16_t u16RequiredSize = u16NumberOfRectangle * u16BytePerRectangleData;

  if (u16SizeCmdOut < u16RequiredSize) {
    ESP_LOGE("xCalcuPointMatrixRectangle", "Output buffer (%u) too small for rectangle data (%u).", u16SizeCmdOut, u16RequiredSize);
    return ESP_ERR_INVALID_ARG;
  }

  uint16_t current_byte_idx = 0;  // Chỉ mục byte hiện tại trong pu8CmdOut

  for (uint16_t r = 0; r < u16Row; ++r) {
    for (uint16_t c = 0; c < u16Col; ++c) {
      uint16_t index_in_1d_color_array = r * u16Col + c;

      // Tính toán tâm hình chữ nhật hiện tại
      int32_t current_center_x = (int32_t)CenterX00 + (c * i16DisX);
      int32_t current_center_y = (int32_t)CenterY00 + (r * i16DisY);

      // Tính toán góc trên bên trái (Xs, Ys)
      // LƯU Ý: DWIN thường dùng tọa độ unsigned. Nếu kết quả âm, có thể gây lỗi.
      // Đảm bảo CenterX00/Y00 đủ lớn hoặc i16DisX/Y không làm kết quả âm.
      int16_t Xs = current_center_x - (int16_t)u16Length / 2;
      int16_t Ys = current_center_y - (int16_t)u16Width / 2;

      // Tính toán góc dưới bên phải (Xe, Ye)
      int16_t Xe = current_center_x + (int16_t)u16Length / 2;
      int16_t Ye = current_center_y + (int16_t)u16Width / 2;

      uint16_t current_color = pu16Color ? pu16Color[index_in_1d_color_array] : 0xF800;

      // Điền dữ liệu cho hình chữ nhật hiện tại vào pu8CmdOut
      // Xs (Big-Endian)
      pu8CmdOut[current_byte_idx++] = (uint8_t)((Xs >> 8) & 0xFF);
      pu8CmdOut[current_byte_idx++] = (uint8_t)(Xs & 0xFF);

      // Ys (Big-Endian)
      pu8CmdOut[current_byte_idx++] = (uint8_t)((Ys >> 8) & 0xFF);
      pu8CmdOut[current_byte_idx++] = (uint8_t)(Ys & 0xFF);

      // Xe (Big-Endian)
      pu8CmdOut[current_byte_idx++] = (uint8_t)((Xe >> 8) & 0xFF);
      pu8CmdOut[current_byte_idx++] = (uint8_t)(Xe & 0xFF);

      // Ye (Big-Endian)
      pu8CmdOut[current_byte_idx++] = (uint8_t)((Ye >> 8) & 0xFF);
      pu8CmdOut[current_byte_idx++] = (uint8_t)(Ye & 0xFF);

      // Color (Big-Endian)
      // LƯU Ý: DWIN spec cho hình chữ nhật (0x0003/0x0004) dùng màu 1 byte.
      // Đoạn code này đang dùng 2 byte (uint16_t). Có thể gây lỗi hoặc sai màu.
      pu8CmdOut[current_byte_idx++] = (uint8_t)((current_color >> 8) & 0xFF);
      pu8CmdOut[current_byte_idx++] = (uint8_t)(current_color & 0xFF);
    }
  }

  return ESP_OK;  // Trả về ESP_OK nếu thành công
}

// --- Hàm xDrawBolderMatrixRetangle (Mới) ---
// Hàm này sẽ tạo lệnh DWIN để vẽ các hình chữ nhật KHÔNG FILL.
esp_err_t DWIN::xDrawBolderMatrixRectangle(
  uint16_t u16VPaddress,
  uint16_t CenterX00,
  uint16_t CenterY00,
  uint16_t u16Length,
  uint16_t u16Width,
  int16_t i16DisX,
  int16_t i16DisY,
  const uint16_t *pu16Color,
  uint16_t u16Row,
  uint16_t u16Col) {

  if (u16Row == 0 || u16Col == 0) {
    ESP_LOGE("xDrawBolderMatrixRectangle", "u16Row == 0 || u16Col == 0");
    return ESP_ERR_INVALID_ARG;
  }
  if (NULL == pu16Color) {
    ESP_LOGW("xDrawBolderMatrixRectangle", "NULL == pu16Color. Using default color.");
  }

  uint16_t u16NumberOfRectangle = u16Row * u16Col;
  if (u16NumberOfRectangle > 24) {  // DWIN max items per command
    ESP_LOGE("xDrawBolderMatrixRectangle", "Number of rectangles (%u) exceeds 255 limit for single command.", u16NumberOfRectangle);
    return ESP_ERR_INVALID_ARG;
  }

  const uint16_t u16GraphicDrawCMD = 0x0003;    // Lệnh vẽ hình chữ nhật (outline)
  const uint16_t u16BytePerRectangleData = 10;  // Xs(2B), Ys(2B), Xe(2B), Ye(2B), Color(2B) = 10 bytes
  uint16_t u16TotalByteRectangleData = u16NumberOfRectangle * u16BytePerRectangleData;

  // Tính kích thước payload DWIN (từ 0x82 đến 0xFF 0x00)
  uint16_t u16PayloadSize = 1 + 2 + 2 + 2 + u16TotalByteRectangleData + 2;

  // Khai báo mảng VLA cho payload DWIN
  uint8_t u8RectanglePayload[u16PayloadSize];
  uint16_t current_payload_idx = 0;

  // Điền các byte đầu tiên của payload DWIN
  u8RectanglePayload[current_payload_idx++] = 0x82;                                           // Command: write (0x82)
  u8RectanglePayload[current_payload_idx++] = (uint8_t)((u16VPaddress >> 8) & 0xFF);          // VP High byte
  u8RectanglePayload[current_payload_idx++] = (uint8_t)((u16VPaddress)&0xFF);                 // VP Low byte
  u8RectanglePayload[current_payload_idx++] = (uint8_t)((u16GraphicDrawCMD >> 8) & 0xFF);     // CMD High byte (0x00)
  u8RectanglePayload[current_payload_idx++] = (uint8_t)((u16GraphicDrawCMD)&0xFF);            // CMD Low byte (0x03)
  u8RectanglePayload[current_payload_idx++] = (uint8_t)((u16NumberOfRectangle >> 8) & 0xFF);  // Quantity High byte
  u8RectanglePayload[current_payload_idx++] = (uint8_t)(u16NumberOfRectangle & 0xFF);         // Quantity Low byte

  // Gọi hàm xCalcuPointMatrixRectangle để điền dữ liệu tọa độ và màu
  esp_err_t err = xCalcuPointMatrixRectangle(
    u8RectanglePayload + current_payload_idx,  // Bắt đầu điền từ vị trí hiện tại trong payload
    u16PayloadSize - current_payload_idx,      // Kích thước còn lại của buffer
    CenterX00, CenterY00, u16Length, u16Width,
    i16DisX, i16DisY, pu16Color, u16Row, u16Col);

  if (err != ESP_OK) {
    return err;  // Trả về lỗi nếu hàm tính toán gặp vấn đề
  }
  // Cập nhật chỉ mục sau khi xCalcuPointMatrixRectangle đã điền dữ liệu
  current_payload_idx += u16TotalByteRectangleData;

  // Điền 0xFF 0x00 kết thúc payload
  u8RectanglePayload[current_payload_idx++] = 0xFF;
  u8RectanglePayload[current_payload_idx++] = 0x00;

  printf("Generated DWIN Bolder Rectangle Command (Total %u bytes):\n", u16PayloadSize);
  for (uint16_t i = 0; i < u16PayloadSize; i++) {
    printf("%02X ", u8RectanglePayload[i]);
  }
  printf("\n");

  return sendArray(u8RectanglePayload, sizeof(u8RectanglePayload) / sizeof(u8RectanglePayload[8]));
}

// --- Hàm xDrawFillMatrixRetangle (Mới) ---
// Hàm này sẽ tạo lệnh DWIN để vẽ các hình chữ nhật CÓ FILL.
esp_err_t DWIN::xDrawFillMatrixRectangle(
  uint16_t u16VPaddress,
  uint16_t CenterX00,
  uint16_t CenterY00,
  uint16_t u16Length,
  uint16_t u16Width,
  int16_t i16DisX,
  int16_t i16DisY,
  const uint16_t *pu16Color,
  uint16_t u16Row,
  uint16_t u16Col) {

  if (u16Row == 0 || u16Col == 0) {
    ESP_LOGE("xDrawFillMatrixRectangle", "u16Row == 0 || u16Col == 0");
    return ESP_ERR_INVALID_ARG;
  }
  if (NULL == pu16Color) {
    ESP_LOGW("xDrawFillMatrixRectangle", "NULL == pu16Color. Using default color.");
  }

  uint16_t u16NumberOfRectangle = u16Row * u16Col;
  if (u16NumberOfRectangle > 24) {  // DWIN max items per command
    ESP_LOGE("xDrawFillMatrixRectangle", "Number of rectangles (%u) exceeds 255 limit for single command.", u16NumberOfRectangle);
    return ESP_ERR_INVALID_ARG;
  }

  const uint16_t u16GraphicDrawCMD = 0x0004;    // Lệnh vẽ hình chữ nhật (fill)
  const uint16_t u16BytePerRectangleData = 10;  // Xs(2B), Ys(2B), Xe(2B), Ye(2B), Color(2B) = 10 bytes
  uint16_t u16TotalByteRectangleData = u16NumberOfRectangle * u16BytePerRectangleData;

  // Tính kích thước payload DWIN (từ 0x82 đến 0xFF 0x00)
  uint16_t u16PayloadSize = 1 + 2 + 2 + 2 + u16TotalByteRectangleData + 2;

  // Khai báo mảng VLA cho payload DWIN
  uint8_t u8RectanglePayload[u16PayloadSize];
  uint16_t current_payload_idx = 0;

  // Điền các byte đầu tiên của payload DWIN
  u8RectanglePayload[current_payload_idx++] = 0x82;                                           // Command: write (0x82)
  u8RectanglePayload[current_payload_idx++] = (uint8_t)((u16VPaddress >> 8) & 0xFF);          // VP High byte
  u8RectanglePayload[current_payload_idx++] = (uint8_t)((u16VPaddress)&0xFF);                 // VP Low byte
  u8RectanglePayload[current_payload_idx++] = (uint8_t)((u16GraphicDrawCMD >> 8) & 0xFF);     // CMD High byte (0x00)
  u8RectanglePayload[current_payload_idx++] = (uint8_t)((u16GraphicDrawCMD)&0xFF);            // CMD Low byte (0x04)
  u8RectanglePayload[current_payload_idx++] = (uint8_t)((u16NumberOfRectangle >> 8) & 0xFF);  // Quantity High byte
  u8RectanglePayload[current_payload_idx++] = (uint8_t)(u16NumberOfRectangle & 0xFF);         // Quantity Low byte

  // Gọi hàm xCalcuPointMatrixRectangle để điền dữ liệu tọa độ và màu
  esp_err_t err = xCalcuPointMatrixRectangle(
    u8RectanglePayload + current_payload_idx,  // Bắt đầu điền từ vị trí hiện tại trong payload
    u16PayloadSize - current_payload_idx,      // Kích thước còn lại của buffer
    CenterX00, CenterY00, u16Length, u16Width,
    i16DisX, i16DisY, pu16Color, u16Row, u16Col);

  if (err != ESP_OK) {
    return err;  // Trả về lỗi nếu hàm tính toán gặp vấn đề
  }
  // Cập nhật chỉ mục sau khi xCalcuPointMatrixRectangle đã điền dữ liệu
  current_payload_idx += u16TotalByteRectangleData;

  // Điền 0xFF 0x00 kết thúc payload
  u8RectanglePayload[current_payload_idx++] = 0xFF;
  u8RectanglePayload[current_payload_idx++] = 0x00;


  printf("Generated DWIN Fill Rectangle Command (Total %u bytes):\n", u16PayloadSize);
  for (uint16_t i = 0; i < u16PayloadSize; i++) {
    printf("%02X ", u8RectanglePayload[i]);
  }
  printf("\n");

  return sendArray(u8RectanglePayload, sizeof(u8RectanglePayload) / sizeof(u8RectanglePayload[8]));
}

esp_err_t DWIN::updateHMI(fs::FS &filesystem, const char *dirPath) {
  /*
       Tắt Serial và bật lại để xóa Serial Event
       do Arduino không có hàm xóa trực tiếp.
       Sau khi cập nhật màn hình dù thành công hay không thì cũng phải restart lại ESP
   */
  // _dwinSerial->end();
  // delayMicroseconds(1000);
  // _dwinSerial->begin(_baudrate, SERIAL_8N1, _rxPin, _txPin);
  // echoEnabled(true);
  /*-----------------------------------------------------*/
  uint8_t hexCode[243];
  int address = 0x8000;
  int i, j;
  uint32_t totalSize = 0;
  int fileIdx = 0;
  String fileName;
  Serial.println("Open: " + String(dirPath));

  /*
        Kiểm tra trong filesystem có tồn tại thư mục Update không ?
        nếu không tồn tại thì thoát khỏi hàm updateHMI
    */
  File root = filesystem.open(dirPath);
  if (!root || !root.isDirectory()) {
    Serial.println("Failed to update");
    return true;
  }

  /*
        Nếu tồn tại thư mục Update trong filesystem thì mở thư mục
        và duyệt qua các file cho đến khi tìm thấy một trong các file
        cập nhật màn hình VD: file .ICL, .BIN, .HZK.
        Thì tiến hành gửi lệnh cập nhật và truyền dữ liệu cập nhật đến DWIN HMI
        Lặp lại việc này cho đến khi nạp hết các file cập nhật vào màn hình
        sau đó restart màn hình để bắt đầu boot lại chương trình mới trên màn hình.
        Nếu không tìm thấy file cập nhật nào thì thoát khỏi hàm
    */
  File file = root.openNextFile();
  Serial.println("Start...");
  while (file) {
    fileName = file.name();
    if (isFirmwareFile(fileName) == false) {
      file.close();
      file = root.openNextFile();
    } else {
      break;
    }
  }
  if (!file) {
    Serial.println(" Not found HMI file");
    return true;
  } else {
    // Gửi lệnh bắt đầu cập nhật đến HMI
    uint8_t hexCodeFlash[] = { 0x82, 0x00, 0xfc, 0x55, 0xaa, 0x5a, 0xa5 };
    if (this->sendArray(hexCodeFlash, sizeof(hexCodeFlash)) != ESP_OK) {
      Serial.println("Failed to update");
      return false;
    }
  }

  /*
        Bắt đầu gửi các mã HEX của các file cập nhật màn hình là các file trong DWIN_SET.
        Mỗi lần gửi 32Kb bắt đầu từ địa chỉ 0x8000 là địa chỉ trên RAM của màn hình.
        Gửi mỗi 240Byte/lần đủ 136 lần thì gửi 128Byte còn lại sau đó gửi lệnh ghi dữ liệu từ RAM vào Flash trên màn hình.
        Gửi lệnh hỏi DWIN đã ghi vào Flash chưa. Nếu DWIN phản hồi đã ghi thành công thì lặp lại quá trình trên cho đến khi hết file.
        Nếu kích thước của file cập nhật không chia hết cho 32Kb thì phần dư của nó cũng sẽ gửi bắt đầu tại địa chỉ 0x8000 trên RAM.
        Flash cũng có địa chỉ địa chỉ của Flash là từ 0-63 tức 64MB Flash được chia thành 64 stack
    */
  while (file) {

    fileName = String(file.name());
    fileIdx = fileName.toInt();
    size_t sizeFile = file.size();

    Serial.print(fileName + ": ");
    Serial.printf("%d Kb, %d byte. Update idx: %d.\n", sizeFile / 1024, sizeFile % 1024, fileIdx);
    Serial.printf("%dx32Kb + %dx240byte + %dbyte\n", sizeFile / 0x8000, sizeFile % 0x8000 / 240, sizeFile % 0x8000 % 240);

    i = 0;

    // Nếu kích thước file chia hết cho 32KB (0x8000)
    if (sizeFile / 0x8000 > 0) {
      // Phần cập nhật từng 32KB
      // sizeFile / 0x8000 = số lần cập nhật mỗi 32KB của file
      for (i = 0; i < sizeFile / 0x8000; i++) {
        // Cập nhật 136 * 240 Byte
        for (j = 0; j < 136; j++) {
          address = 0x8000 + j * 0x78;
          hexCode[0] = 0x82;
          hexCode[1] = (uint8_t)((address >> 8) & 0xFF);
          hexCode[2] = (uint8_t)((address)&0xFF);
          totalSize += file.read((uint8_t *)(hexCode + 3), 240);

          if (this->sendArray(hexCode, 243) != ESP_OK) {
            file.close();
            return false;
          }
        }

        // Cập nhật 128Kb còn lại tức là đã cập nhật (136*240 + 128) * i = 32Kb * i
        address = 0x8000 + j * 0x78;
        hexCode[0] = 0x82;
        hexCode[1] = (uint8_t)((address >> 8) & 0xFF);
        hexCode[2] = (uint8_t)((address)&0xFF);
        totalSize += file.read((uint8_t *)(hexCode + 3), 128);

        if (this->sendArray(hexCode, 0x83) != ESP_OK) {
          file.close();
          return false;
        }

        // Gửi lệnh ghi dữ liệu từ RAM vào FLash
        int FlashAddrToWrite = fileIdx * 8 + i;
        {
          uint8_t hexCodeFlash[] = { 0x82, 0x00, 0xaa, 0x5a, 0x02, (uint8_t)((FlashAddrToWrite >> 8) & 0xFF), (uint8_t)((FlashAddrToWrite)&0xFF), 0x80, 0x00, 0x17, 0x70, 0x00, 0x00, 0x00, 0x00 };

          if (this->sendArray(hexCodeFlash, sizeof(hexCodeFlash)) != ESP_OK) {
            file.close();
            return false;
          }
        }
        delay(200);  // Chờ 200ms để màn hình ghi dữ liệu từ RAM vào Flash

        // Kiếm tra xem dữ liệu đã ghi xong chưa nếu xong màn hình sẽ phản hồi về mã "aa 01 00 02"
        // Nếu phản hồi về mã khác thì chờ 100ms ròi tiếp tra tiếp lần tiếp theo tối đa 3 lần
        // Mà không nhận được mã "aa 01 00 02" tức cập nhật thất bại
        for (int k = 0; true; k++) {
          const uint8_t hexCodeFlash[] = { 0x83, 0x00, 0xaa, 0x01 };
          uint16_t u16SizeOfResponeBuffer = u6CalcuSizeOfResponeBuffer(hexCodeFlash, sizeof(hexCodeFlash) / sizeof(hexCodeFlash[0]));
          uint8_t pu8Data[u16SizeOfResponeBuffer] = {};
          this->sendArray(hexCodeFlash, sizeof(hexCodeFlash), pu8Data, u16SizeOfResponeBuffer);
          if (pu8Data[8] == 0x02) {
            break;
          }

          else if (k >= 3) {
            file.close();
            return false;
          }
          delayMicroseconds(100000);
        }
      }
    }

    // Gửi những file có kích thước dưới 32Kb hoặc phần dư còn lại của file có kích thước > 32Kb
    j = 0;
    if ((sizeFile % 0x8000) / 240 > 0) {
      for (j = 0; j < (sizeFile % 0x8000) / 240; j++) {
        address = 0x8000 + j * 0x78;
        hexCode[0] = 0x82;
        hexCode[1] = (uint8_t)((address >> 8) & 0xFF);
        hexCode[2] = (uint8_t)((address)&0xFF);

        totalSize += file.read((uint8_t *)(hexCode + 3), 240);
        // Serial.write(hexCode, 0xF3);
        if (this->sendArray(hexCode, 0xF3) != ESP_OK) {
          file.close();
          return false;
        }
      }
    }
    if ((sizeFile % 0x8000) % 240 > 0) {
      address = 0x8000 + j * 0x78;
      hexCode[0] = 0x82;
      hexCode[1] = (uint8_t)((address >> 8) & 0xFF);
      hexCode[2] = (uint8_t)((address)&0xFF);
      totalSize += file.read((uint8_t *)(hexCode + 3), (sizeFile % 0x8000) % 240);
      if (this->sendArray(hexCode, (sizeFile % 0x8000) % 240 + 3) != ESP_OK) {
        file.close();
        return false;
      }
    }

    int FlashAddrToWrite = fileIdx * 8 + sizeFile / 0x8000;
    {
      uint8_t hexCodeFlash[] = { 0x82, 0x00, 0xaa, 0x5a, 0x02, (uint8_t)((FlashAddrToWrite >> 8) & 0xFF), (uint8_t)((FlashAddrToWrite)&0xFF), 0x80, 0x00, 0x17, 0x70, 0x00, 0x00, 0x00, 0x00 };
      if (this->sendArray(hexCodeFlash, sizeof(hexCodeFlash)) != ESP_OK) {
        file.close();
        return false;
      }
    }
    delay(200);
    for (int k = 0; true; k++) {
      const uint8_t hexCodeFlash[] = { 0x83, 0x00, 0xaa, 0x01 };
      uint16_t u16SizeOfResponeBuffer = u6CalcuSizeOfResponeBuffer(hexCodeFlash, sizeof(hexCodeFlash) / sizeof(hexCodeFlash[0]));
      uint8_t pu8Data[u16SizeOfResponeBuffer] = {};
      this->sendArray(hexCodeFlash, sizeof(hexCodeFlash), pu8Data, u16SizeOfResponeBuffer);
      if (pu8Data[8] == 0x02) {
        break;
      } else if (k >= 3) {
        file.close();
        return false;
      }
      delayMicroseconds(100000);
    }

    file.close();
    file = root.openNextFile();
    // Kiểm tra file tiếp theo trong thư mục Update có phải là file cập nhật màn hình không
    // Nếu là file cập nhật màn hình thì break khỏi vòng lặp để bắt đầu nạp file này vào màn hình
    // Nếu không còn file nào thì thoát và kết thúc quá trình nạp màn hình
    while (file) {
      fileName = file.name();
      if (isFirmwareFile(fileName) == false) {
        file.close();
        file = root.openNextFile();
      } else {
        break;
      }
    }
  }

  // Đóng các file đã mở trên filesystem
  if (root) {
    root.close();
  }
  if (file) {
    file.close();
  }

  // Restart lại màn hình
  Serial.println("RestartHMI");
  this->restartHMI();
  Serial.println("Done!");
  Serial.printf("Size: %d", totalSize);
  return true;
}

#pragma region private
uint16_t DWIN::u6CalcuSizeOfResponeBuffer(const uint8_t *sendBuffer, uint16_t u16SizeOfBuffer) {
  return MAX_RESPONE_LENGTH;
}

bool DWIN::isFirmwareFile(String &fileName) {
  const char *const pcCONST_END_FILE[] = {
    ".bin", ".BIN", ".icl", ".ICL", ".dzk", ".DZK",
    ".hzk", ".HZK", ".lib", ".LIB", ".wae", ".WAE",
    ".uic", ".UIC", ".gif", ".GIF"
  };
  for (const auto &ext : pcCONST_END_FILE) {
    if (fileName.endsWith(ext)) {
      return true;
    }
  }
  return false;
}
/**
 *@brief wait until TX have done
 *
 */
void DWIN::flushSerial() {
  _dwinSerial->flush();
}
void DWIN::clearSerial() {
  while (_dwinSerial->available()) {
    _dwinSerial->read();
  }
}
uint16_t DWIN::u16CalculateCRCModbus(uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;

  if (data == NULL) {
    return 0;
  }
  for (size_t i = 0; i < length; ++i) {
    crc ^= data[i];

    for (uint8_t j = 0; j < 8; ++j) {
      if (crc & 0x01) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

void DWIN::xTaskReceiveUartEvent(void *ptr) {
  if (ptr == NULL) {
    abort();
  }
  DWIN *pxDWIN = (DWIN *)ptr;
  uint32_t notifyNum;
  while (1) {
    xTaskNotifyWait(pdFALSE, pdTRUE, &notifyNum, portMAX_DELAY);
    pxDWIN->xHandle();
  }
}
void DWIN::xHandle() {
  uint8_t pu8BufferReceiver[MAX_RESPONE_LENGTH] = {};
  TouchFrame_t xTouchData = {};
  while (_dwinSerial->available()) {
    if (_dwinSerial->read() != CMD_HEAD1) {
      continue;
    }
    if (_dwinSerial->read() != CMD_HEAD2) {
      continue;
    }
    pu8BufferReceiver[FRAME_HEADER_POSTION1] = CMD_HEAD1;
    pu8BufferReceiver[FRAME_HEADER_POSTION2] = CMD_HEAD2;
    uint16_t length = _dwinSerial->read();
    pu8BufferReceiver[LENGTH_SEND_CMD_POSITION] = length;
    _dwinSerial->readBytes(pu8BufferReceiver + CMD_POSITION, length);

    if (_crc) {
      uint16_t u16ReceivedCRC;
      memcpy((uint8_t *)&u16ReceivedCRC, &pu8BufferReceiver[length + 1], 2);
      uint16_t u16CalculatedCRC = u16CalculateCRCModbus(pu8BufferReceiver + CMD_POSITION, length - 2);
      if (u16ReceivedCRC != u16CalculatedCRC) {
        ESP_LOGE(__func__, "DWIN frame CRC check failed. Received: 0x%04X, Calculated: 0x%04X", u16ReceivedCRC, u16CalculatedCRC);
        continue;
      }
    }

    printfArrDebug(pu8BufferReceiver, 50);


    PendingRequest_t xPendingRequest;
    uint16_t u16VPAddressReceive = _GET_VP_ADDRESS(pu8BufferReceiver, MAX_RESPONE_LENGTH);

    uint8_t cmd = pu8BufferReceiver[CMD_POSITION];
    if (xQueuePeek(xQueueCommandReadWrite, &xPendingRequest, 0) && cmd == xPendingRequest.u8cmd) {
      if (cmd == CMD_WRITE || ((cmd == CMD_READ) && u16VPAddressReceive == xPendingRequest.u16VPaddress)) {
        xQueueSend(xPendingRequest.xQueueGetResponeData, pu8BufferReceiver, 10);
        continue;
      }
    }

    xTouchData.u16VPaddress = u16VPAddressReceive;
    xTouchData.u16KeyValue = (((uint16_t)pu8BufferReceiver[7]) << 8) | pu8BufferReceiver[8];
    xQueueSend(xQueueTouch, &xTouchData, 50);

    delay(1);  // reset watchdog
  }
}
void DWIN::vTriggerTaskReceiveFromUartEvent() {
  xTaskNotify(xTaskDWINHandle, 0x01, eSetBits);
}