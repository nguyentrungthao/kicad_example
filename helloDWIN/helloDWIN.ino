#include "DWIN.h"

#define TX 21
#define RX 47

DWIN _dwin(Serial2, RX, TX, 115200);
QueueHandle_t xQueueTouch;

void xHMITouchTask(void *ptr) {
  TouchFrame_t touchRev;
  while (true) {
    xQueueReceive(xQueueTouch, &touchRev, portMAX_DELAY);
    Serial.printf("VP %x, Key %x\n", touchRev.u16VPaddress, touchRev.u16KeyValue);
  }
}
const uint16_t pu16DWIN_COLOR[] = { 0xF800, 0x07E0, 0x001F, 0xFFE0, 0x07FF, 0xF81F, 0xFFFF, 0x0000, 0xAEDF };
enum Color_t {
  eCOLOR_RED = 0U,
  eCOLOR_GREEN = 1U,
  eCOLOR_BLUE = 2U,
  eCOLOR_YELLOW = 3U,
  eCOLOR_CYAN = 4U,
  eCOLOR_MAGENTA = 5U,
  eCOLOR_WHILE = 6U,
  eCOLOR_BLACK = 7U,
  eBACK_GROUND = 8U
};
uint16_t pu16Color[8][6] = {
  { pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN] },
  { pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN] },
  { pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN] },
  { pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN] },
  { pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN] },
  { pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN] },
  { pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN] },
  { pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN], pu16DWIN_COLOR[eCOLOR_GREEN] },
};

const uint16_t pu16ColorBackGround[8][6] = {
  { pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND] },
  { pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND] },
  { pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND] },
  { pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND] },
  { pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND] },
  { pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND] },
  { pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND] },
  { pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND], pu16DWIN_COLOR[eBACK_GROUND] },
};

#define DIS_X 65
#define DIS_Y 65
#define RADIUS 25

#define X00 70
#define Y00 550

#define X05 (X00 + (5 * DIS_X))
#define Y05 Y00

#define X40 X00
#define Y40 (Y00 + (3 * -DIS_Y))

#define X45 (X00 + (4 * DIS_X))
#define Y45 (Y00 + (3 * -DIS_Y))
uint16_t *pu16COLOR_MATRIX = (uint16_t *)pu16Color;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  _dwin.begin();
  _dwin.restartHMI();
  _dwin.crcEnabled(true);  //! chú ý phần này sẽ phục thuộc vào cấu hình trong CFG edit
  _dwin.setupTouchCallBack(&xQueueTouch, 10);
  xTaskCreatePinnedToCore(xHMITouchTask, "HMItouch", 4096, (void *)NULL, configMAX_PRIORITIES - 2, NULL, tskNO_AFFINITY);

  _dwin.setPage(73);

  // _dwin.xDrawMatrixCircle(0x837C, X00, Y00, RADIUS, DIS_X, -DIS_Y, (const uint16_t *)pu16COLOR_MATRIX, 4, 6);
  // _dwin.xDrawMatrixCircle(0x857C, X40, Y40, RADIUS, DIS_X, -DIS_Y, (const uint16_t *)(pu16COLOR_MATRIX + 24), 4, 6);
  // _dwin.xDrawMatrixCircle(0x847C, X00, Y00, 0, DIS_X, -DIS_Y, (const uint16_t *)pu16COLOR_MATRIX, 4, 6);
  // _dwin.xDrawMatrixCircle(0x867C, X40, Y40, 0, DIS_X, -DIS_Y, (const uint16_t *)(pu16COLOR_MATRIX + 24), 4, 6);

  delay(1000);
}

uint32_t t = 0;
uint32_t cout = 0;
bool st = false;

void loop() {
  // put your main code here, to run repeatedly:

  if (Serial.available()) {
    String str = Serial.readString();
    uint8_t outputBuffer3[260];
    size_t actualBytes3 = convertHexStringToByteArray(str, outputBuffer3, sizeof(outputBuffer3));
    for (size_t i = 0; i < actualBytes3; i++) {
      if (outputBuffer3[i] < 0x10) Serial.print("0");
      Serial.print(outputBuffer3[i], HEX);
      Serial.print(" ");
    }
    Serial.print("=> ");
    if (_dwin.sendArray(outputBuffer3, actualBytes3) == ESP_OK) {
      Serial.println("OKE");
    } else {
      Serial.println("ERR");
    }
  }

  // if (millis() - t > 1000) {
  //   if (st) {
  //     pu16COLOR_MATRIX = (uint16_t *)pu16Color;
  //     st = false;
  //   } else {
  //     pu16COLOR_MATRIX = (uint16_t *)pu16ColorBackGround;
  //     st = true;
  //   }
  //   _dwin.xDrawMatrixCircle(0x847C, X00, Y00, 0, DIS_X, -DIS_Y, (const uint16_t *)pu16COLOR_MATRIX, 4, 6);
  //   _dwin.xDrawMatrixCircle(0x867C, X40, Y40, 0, DIS_X, -DIS_Y, (const uint16_t *)(pu16COLOR_MATRIX + 24), 4, 6);
  //   t = millis();
  // }
}


// Hàm chuyển đổi một ký tự hex (0-9, A-F, a-f) thành giá trị số nguyên
uint8_t hexCharToByte(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return 0;  // Trả về 0 nếu không phải ký tự hex hợp lệ (có thể xử lý lỗi tốt hơn)
}

// Hàm chuyển đổi chuỗi String chứa các giá trị hex, cách nhau bởi dấu cách
// thành mảng các byte hex. Hàm này sẽ xử lý các giá trị 2 ký tự và 4 ký tự hex.
// Ví dụ: "82 05 AB CDEF" sẽ thành {0x82, 0x05, 0xAB, 0xCD, 0xEF}
// Đầu vào:
//   hexString: Chuỗi String chứa các giá trị hex, cách nhau bởi dấu cách.
//              Ví dụ: "5A A5 07 82 000C 5A00 0150"
//   outputArray: Con trỏ tới mảng byte (uint8_t) để lưu kết quả.
//   maxOutputLength: Kích thước tối đa của outputArray để tránh tràn bộ đệm.
// Trả về: Số lượng byte thực tế đã được chuyển đổi và lưu vào outputArray.
size_t convertHexStringToByteArray(String hexString, uint8_t *outputArray, size_t maxOutputLength) {
  size_t byteCount = 0;
  hexString.trim();  // Xóa khoảng trắng ở đầu và cuối chuỗi

  int startIndex = 0;
  int spaceIndex = hexString.indexOf(' ');

  while (startIndex < hexString.length() && byteCount < maxOutputLength) {
    String hexToken;
    if (spaceIndex == -1) {  // Đây là phần tử cuối cùng
      hexToken = hexString.substring(startIndex);
    } else {
      hexToken = hexString.substring(startIndex, spaceIndex);
    }

    // Xử lý từng token (ví dụ "82", "000C")
    // Mỗi token phải có độ dài chẵn để biểu diễn các byte hex
    if (hexToken.length() % 2 != 0) {
      // Xử lý lỗi: token có độ dài lẻ không phải là hex byte hợp lệ
      // Bạn có thể bỏ qua hoặc báo lỗi tùy theo yêu cầu
      startIndex = spaceIndex + 1;
      spaceIndex = hexString.indexOf(' ', startIndex);
      continue;
    }

    for (int i = 0; i < hexToken.length(); i += 2) {
      if (byteCount < maxOutputLength) {
        uint8_t highNibble = hexCharToByte(hexToken.charAt(i));
        uint8_t lowNibble = hexCharToByte(hexToken.charAt(i + 1));
        outputArray[byteCount++] = (highNibble << 4) | lowNibble;
      } else {
        // Đã đạt đến maxOutputLength, dừng lại để tránh tràn bộ đệm
        return byteCount;
      }
    }

    if (spaceIndex == -1) {  // Đã xử lý phần tử cuối cùng
      break;
    }
    startIndex = spaceIndex + 1;
    spaceIndex = hexString.indexOf(' ', startIndex);
  }
  return byteCount;
}