# Kiến thức thiết kế sơ đồ nguyên lý

- **Kiến thức cơ bản về điện tử**: [Playlist YouTube](https://www.youtube.com/playlist?list=PLGs3LoDo-2kOWhcLkcdEZovVq_vuPsPHr)
- **Kiến thức về các chuẩn giao tiếp của MCU**: I2C, SPI, UART
- **Kiến thức về nguồn**:
  - **Mạch lọc EMI**: [Video YouTube](https://www.youtube.com/watch?v=QAZo9xbiflQ&t=97s)
  - **Nguồn (Switching Power Supply)**: [Video YouTube](https://www.youtube.com/watch?v=2N_T7VFymFg)
  - **Nguồn xung Buck (Buck Converter)**: [Video YouTube](https://www.youtube.com/watch?v=W6NOV6b8kxs)

---

# KICAD

## Setup phần mềm cài plugin JLC và IBOM

### IBOM:
- [GitHub IBOM Plugin](https://github.com/openscopeproject/InteractiveHtmlBom)
- ![IBOM Plugin](https://github.com/nguyentrungthao/kicad_example/blob/main/Document/install_IBOM.png?raw=true)

### JLCPCB:
- [GitHub JLCPCB Plugin](https://github.com/Bouni/kicad-jlcpcb-tools)
- **Link điền ở mục 4**: '''https://raw.githubusercontent.com/Bouni/bouni-kicad-repository/main/repository.json'''
- ![JLCPCB Plugin](https://github.com/nguyentrungthao/kicad_example/blob/main/Document/install_JLC.png?raw=true)

---

## Thiết kế phần cứng 
- $${\color{red} NHỚ ĐỌC TÀI LIỆU CỦA LINH KIỆN TRƯỚC KHI VẼ}$$

##### Hướng dẫn thêm thư viện IoTVision vào KICAD

1. **Vào KICAD**.
2. **Chọn Preferences → Configure Paths**.
3. **Thêm đường dẫn thư viện**:
    - **3D**: Thêm alias như **IOTVISION_3D** và trỏ đường dẫn đến thư mục 3D step.
    - **Symbols**: Thêm alias như **IOTVISION_SYMBOL_DIR** và trỏ đường dẫn đến thư mục symbols.
    - **Footprints**: Thêm alias như **IOTVISION_FOOTPRINT_DIR** và trỏ đường dẫn đến thư mục footprints.
4. Nhấn **OK**.
5. **Quản lý thư viện**:
    - **Preferences → Manage Symbol Libraries → Global Libraries**.
    - Thêm **IVS_SYMBOLS** và đường dẫn thư viện **Symbols.kicad_sym**.
6. **Quản lý Footprint Libraries**:
    - Thêm **IVS_FOOTPRINTS** và đường dẫn **IVSFootprint.pretty**.

> **Lưu ý**: Nếu mở **PCB Editor** hoặc **Footprint Editor**, bạn sẽ không thấy **Manage Symbol Libraries**. 

- **Video mẫu**: [Hướng dẫn cài thư viện](https://youtu.be/sz3mICmpLzM)

---
#### Schematic

1. **Lấy linh kiện**.
2. **Định nghĩa Bus I2C, SPI, Bus số (Demo vẽ insert)**.
3. **Sử dụng Netclass** (định nghĩa class, màu mè cho class).
4. **Thêm hình ảnh** vào Schematic.
5. **Thêm Footprint** cho linh kiện.
6. **Thêm giá trị linh kiện**.
7. **Vẽ Schematic**:
   - Đối với những bạn mới, nên thêm **Solder Jumper** để hạn chế cháy linh kiện.
8. **Thêm mã BOM** cho JLC, Thế Giới IC, IC Đầy Rối.
9. **Xuất BOM**.
10. **Xuất PDF**.

### Các bước khác:
- **Sửa và cập nhật symbol**.
- **Exclude linh kiện khỏi BOM**.
- **Xuất symbol vào thư viện riêng**.

---

#### PCB Design

1. **Thêm thư viện** vào PCB.
2. **Cấu hình board setup**:
   - Thêm/xóa các lớp (4 lớp, 6 lớp).
   - Cấu hình chữ, **Designed Rules**.
   - Cấu hình kích thước track, via, các netclass.
3. **Vẽ viền board** (đúng mm hoặc nhập file DXF).
4. **Vẽ kích thước board**, kích thước vị trí lỗ ốc.
5. **Tạo grid** tùy chỉnh.
6. **Cắt board tạo khoảng trống cách ly**.
7. **Update SCH sang PCB**.
8. **Sửa footprint trực tiếp**.
9. **Layout linh kiện**:
   - Xếp theo chụp các linh kiện, dùng các lệnh sắp xếp linh kiện.
   - Đặt connecter theo vỏ hộp từ đó đặt các linh kiện còn lại.
   - Chia vùng điện áp, vùng phủ đồng và giải thích các lựa chọn phủ đồng: thermal => sold, zone priority.
   - Dùng clearance và giải thích.
   - Rọc lớp đồng để phủ chì.
   - Sắp xếp tinh các linh kiện theo sơ đồ nguyên lý.
   - Đi dây:
     - Đi dây vi sai.
     - Hướng dẫn matching length.
     - Tính trở kháng đường dây.
     - Thêm các via GND.
     - Thêm teardrop.
10. **Xếp chữ ghi, chữ nổi đậm**, edit font chữ nhanh thêm thông tin, logo.
11. **Chạy DRC kiểm lỗi**.
12. **Xuất STEP**.
13. **Xuất PDF**.
14. **Xuất GERBER**.
15. **Xuất CPL**.
16. **Update file lên JLC để review**.

---

### Một số lỗi

- Viền board không nhận => chạy DRC sửa vị trí đỏ.

---
### Danh sách phát video kicad [Kicad play list](https://www.youtube.com/playlist?list=PLl72apUE_izgVsnWJDfx34R6ak8Qz5oGz) 

## HÀN MẠCH

- Học hàn, khò với thiết hàn cuộn và kem chì.
- Học cách tháo connector.

---

## DO NGUỘI

- Đo thông mạch nguồn, các đường tín hiệu trước khi cắm điện.

---

## LẬP TRÌNH TEST MẠCH

- Test nạp code ESP32 => nếu không tự động nạp được, xem lại mạch nút nhấn BOOT và RESET của ESP32.
- Viết code test từng module trên PCB.
