 /*
 * Nhịp thường
 * Nhịp chậm: nhịp xoang chậm, nhịp bộ nối, nhịp block nhĩ thất độ I
 * Nhịp nhanh: nhịp nhanh xoang, nhịp nhanh nhĩ, rung nhĩ 4:1
 * Nhịp nhanh thất
 */
// Khai bao thu vien
#include "SPI.h"
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
// Dinh nghia khoang tin hieu
#define INIT 0
#define IDLE 1
#define QRS 2
// Khai bao cac hang so khac nhau trong dang song
#define FOUR 4
#define THREE 3
#define TWO 2
#define ONE 1
// Khai bao chan encoder
const int CLK = 26;
const int DT = 28;
// Khai bao cac bien encoder
int counter = 0;
int currentStateCLK;
int lastStateCLK;
// Khai bao tan so dieu chinh cua cac tin hieu nhip nhanh
float frequency_flu = 94.78;
float frequency_ven = 79;
// Khai bao chan cac ket noi
const int buz = 24;
const int select = 30;
const int back = 32;
const int sens = 34;
// Khai bao cac bien nut nhan
int demSelect = 0;
int senses = 1;
int menu1 = 0;
int menu_bra = 0;
int menu_sup = 0;
int valBack;
int valBackold = HIGH;
int valSelect;
int valSelectold = HIGH;
int valSense;
int valSenseold = HIGH;
// Khai bao nhip mac dinh cua cac tin hieu dien tam do
int Bpm_select = 0;
int Bpm_normal = 75;          // Nhip mac dinh cua nhip tim thuong (60-100)
int Bpm_slow = 50;            // Nhip mac dinh cua nhip tim cham (25 - 55)
int Bpm_junc = 50;            // Nhip mac dinh cua nhip bo noi (40 -60)
int Bpm_block = 65;           // Nhip mac dinh cua block nhi that do I (60-100)
int Bpm_sinus = 130;          // Nhip mac dinh cua nhip nhanh (120-240)
int Bpm_atrial = 160;         // Nhip mac dinh cua nhip nhanh nhi (100-250)
int Bpm_flutter = 120;        // Nhip mac dinh cua nhip cuong nhi (100-160)
int Bpm_ven = 140;            // Nhip mac dinh cua nhip nhanh that (140-280)
// Khai bao cac bien toan cuc
unsigned int QRSCount = 0;    // Thoi gian chu ki QRS hoat dong (msec)
unsigned int IdleCount = 0;   // Thoi gian chu ki Idle hoat dong (msec)
unsigned long IdlePeriod = 0; // IdlePeriod duoc dieu chinh de cai dat nhip tim
unsigned int State = INIT;    // Trang thai la INIT, QRS, Idle
unsigned int tcnt2;           // Tai lai gia tri Timer2
// Khai bao cac bien lien quan den dieu chinh thong so nhip
int BeatsPerMinute = 0;       // Gia tri nhip tim duoc gan trong qua trinh tinh toan Numsample
int pembagi = 0;              // He so chia ti le toc do lay mau
float sense = 0.24;           // Gia tri mac dinh cua do nhay
unsigned int NumSample;       // Kich thuoc mau tính theo cong thuc (NumSample = kich thuoc mau/(2+pembagi/100))
int number = -1;              // Thiet lap ban dau cua gia tri de chon truong hop nhip tim
short data_0[537] = {};       // Khai bao mang du lieu (533: tổng dữ liệu roi rac cua dien tam do), nhip thuong va nhip thap
//short data_0[0] = {}; 
short data_1[0] = {};         // Khai bao mang du lieu nhip boi noi
//short data_1[567] = {};
short data_2[0] = {};         // Khai bao mang du lieu nhip block nhi that do I
//short data_2[662] = {};
//short data_3[0] = {};         // Khai bao mang du lieu nhip nhanh
short data_3[380] = {};            
//short data_4[0] = {};       // Khai bao mang du lieu nhip nhanh nhi
short data_4[363] = {};         
//short data_5[0] = {};       // Khai bao mang du lieu nhip cuong nhi
short data_5[536] = {};
//short data_6[0] = {};       // Khai bao mang du lieu nhip nhanh that
short data_6[260] = {};

LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup(){
  Serial.begin(115200);
// Thiet lap ban dau LCD
  lcd.init();
  lcd.backlight();
// Thiet lap chan nut nhan
  pinMode(select, INPUT_PULLUP);
  pinMode(back, INPUT_PULLUP);
  pinMode(sens, INPUT_PULLUP);
  pinMode(buz, OUTPUT);
// Thiet lap chan SPI
  pinMode(53, OUTPUT);
  pinMode(51, OUTPUT);
  pinMode(52, OUTPUT);
// Thiet lap encoder
  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  lastStateCLK = digitalRead(CLK);
// Thiet lap giao dien hien thi
  lcd.clear();
  lcd.setCursor(3, 1);
  lcd.print("DESIGN BY VHT");
  delay(2000);
  lcd.clear();
// Thiet lap ky tu ">"
  byte arrowPattern[] = {
      B10000,
      B11000,
      B11100,
      B11110,
      B11100,
      B11000,
      B10000,
      B00000,
  };
  lcd.createChar(0, arrowPattern);
// Thiet lap giao dien SPI
  SPI.begin();
  SPI.setDataMode(0);
  SPI.setClockDivider(SPI_CLOCK_DIV64);
  SPI.setBitOrder(MSBFIRST);
  TIMSK2 &= ~(1 << TOIE2);
  TCCR2A &= ~((1 << WGM21) | (1 << WGM20));
  TCCR2B &= ~(1 << WGM22);
  ASSR &= ~(1 << AS2);
  TIMSK2 &= ~(1 << OCIE2A);
  TCCR2B |= (1 << CS22) | (1 << CS20);
  TCCR2B &= ~(1 << CS21);
  tcnt2 = 131;
  TCNT2 = tcnt2;
  TIMSK2 |= (1 << TOIE2);
// Hien thi man hinh giao dien chinh
  manhinh();
}
// THIET LAP MAN HINH GIAO DIEN CHINH
void manhinh(){
  lcd.clear();
  lcd.setCursor(2, 1);
  lcd.print("MO HINH MO PHONG");
  lcd.setCursor(4, 2);
  lcd.print("TIN HIEU ECG");
}
// Thiet lap giao dien tuy chinh NHOM BENH
void menu_1(){
  lcd.clear();
  lcd.setCursor(0, 0);
  const char *options[] = {" NHIP THUONG", " NHIP CHAM", " NHIP NHANH", " NHIP NHANH THAT"};
  const int displayCount = 4;
  int startIdx = max(0, menu1 - displayCount + 1);
  if (menu1 >= startIdx + displayCount){
    startIdx = menu1 - displayCount + 1;
  }
  for (int i = 0; i < displayCount; ++i){
    int optionIdx = startIdx + i;
    if (optionIdx >= 0 && optionIdx < sizeof(options) / sizeof(options[0])){
      lcd.setCursor(i == menu1 - startIdx ? 0 : 1, i);
      if (optionIdx == menu1){
        lcd.write((uint8_t)0);
      }
      lcd.print(options[optionIdx]);
    }
  }
}
// Thiet lap giao dien tuy chinh NHIP CHAM
void Bradycardia(){
  lcd.clear();
  lcd.setCursor(0, 0);
  const char *options[] = {" NHIP XOANG CHAM", " NHIP BO NOI", " BLOCK AV DO I"};
  const int displayCount = 3;
  int startIdx = max(0, menu_bra - displayCount + 1);
  if (menu_bra >= startIdx + displayCount){
    startIdx = menu_bra - displayCount + 1;
  }
  for (int i = 0; i < displayCount; ++i){
    int optionIdx = startIdx + i;
    if (optionIdx >= 0 && optionIdx < sizeof(options) / sizeof(options[0])){
      lcd.setCursor(i == menu_bra - startIdx ? 0 : 1, i);
      if (optionIdx == menu_bra){
        lcd.write((uint8_t)0);
      }
      lcd.print(options[optionIdx]);
    }
  }
}
// Thiet lap giao dien tuy chinh NHIP NHANH
void Supraven(){
  lcd.clear();
  lcd.setCursor(0, 0);
  const char *options[] = {" NHIP XOANG NHANH", " NHIP NHANH NHI", " RUNG NHI 4:1"};
  const int displayCount = 3;
  int startIdx = max(0, menu_sup - displayCount + 1);
  if (menu_sup >= startIdx + displayCount){
    startIdx = menu_sup - displayCount + 1;
  }
  for (int i = 0; i < displayCount; ++i){
    int optionIdx = startIdx + i;
    if (optionIdx >= 0 && optionIdx < sizeof(options) / sizeof(options[0])){
      lcd.setCursor(i == menu_sup - startIdx ? 0 : 1, i);
      if (optionIdx == menu_sup){
        lcd.write((uint8_t)0);
      }
      lcd.print(options[optionIdx]);
    }
  }
}
// Thiet lap gia tri BPM va cac gia tri khac
void set_beats_per_minute(int menu_value, int bpm_values[], int pembagi_values[], int num_values){
  if (menu_value >= 0 && menu_value < num_values){
    BeatsPerMinute = bpm_values[menu_value];
    pembagi = pembagi_values[menu_value];
  }
}
// THIET LAP CHON NHIP THUONG MENU 1
void chon_menu1(){
  int bpm_values[] = {Bpm_normal, 0, 0, Bpm_ven};
  int pembagi_values[] = {0, 0, 0, 70};
  set_beats_per_minute(menu1, bpm_values, pembagi_values, 4);
}
// THIET LAP CHON NHIP CHAM MENU BRA
void chon_menu_bra(){
  int bpm_values[] = {Bpm_slow, Bpm_junc, Bpm_block};
  int pembagi_values[] = {0, 0, 0};
  set_beats_per_minute(menu_bra, bpm_values, pembagi_values, 3);
}
// THIET LAP CHON NHIP NHANH MENU SUP
void chon_menu_sup(){
  int bpm_values[] = {Bpm_sinus, Bpm_atrial, Bpm_flutter};
  int pembagi_values[] = {0, 0, 20};
  set_beats_per_minute(menu_sup, bpm_values, pembagi_values, 3);
}
// Thiet lap giao dien hien thi va dieu chinh nhip
void displayMenu(int titleX, String title){
  lcd.clear();
  lcd.setCursor(titleX, 0);
  lcd.print(title);
  lcd.setCursor(0, 1);
  lcd.print("BPM = ");
  lcd.setCursor(0, 2);
  lcd.print("DO NHAY = 1.0mV");
}
// Thiet lap encoder dieu chinh nhip
void adjustBeatsPerMinute(int minBPM, int maxBPM){
  noInterrupts();
  IdlePeriod = (unsigned int)((float)60000.0 / BeatsPerMinute) - (float)NumSample;
  interrupts();
  currentStateCLK = digitalRead(CLK);
  if (currentStateCLK != lastStateCLK && currentStateCLK == 1){
    if (digitalRead(DT) != currentStateCLK){
      BeatsPerMinute = (BeatsPerMinute <= minBPM) ? minBPM : BeatsPerMinute - 1;
    }
    else{
      BeatsPerMinute = (BeatsPerMinute >= maxBPM) ? maxBPM : BeatsPerMinute + 1;
    }
    buzzer();
  }
  lastStateCLK = currentStateCLK;
  lcd.setCursor(6, 1);
  lcd.print(BeatsPerMinute);
  lcd.print(" NHIP/PHUT  ");
  general();
}
// Thiet lap dieu chinh nhip nhanh khong co khoang nhan roi
void adjustFrequency(int minF, int maxF){
  noInterrupts();
  IdlePeriod = (unsigned int)((float)60000.0 / BeatsPerMinute) - (float)NumSample;
  interrupts();
  currentStateCLK = digitalRead(CLK);
  if (currentStateCLK != lastStateCLK && currentStateCLK == 1){
    if (digitalRead(DT) != currentStateCLK){
      BeatsPerMinute = (BeatsPerMinute <=  minF) ?  minF : BeatsPerMinute - 1;
      frequency_ven -= 0.6;
    }
    else{
      BeatsPerMinute = (BeatsPerMinute >= maxF) ? maxF : BeatsPerMinute + 1;
      frequency_ven += 0.6;
    }
    buzzer();
  }
  lastStateCLK = currentStateCLK;
  lcd.setCursor(6, 1);
  lcd.print(BeatsPerMinute);
  lcd.print(" NHIP/PHUT  ");
  general();
}
void adjustFrequency_1(int minF1, int maxF1){
  noInterrupts();
  IdlePeriod = (unsigned int)((float)60000.0 / BeatsPerMinute) - (float)NumSample;
  interrupts();
  currentStateCLK = digitalRead(CLK);
  if (currentStateCLK != lastStateCLK && currentStateCLK == 1){
    if (digitalRead(DT) != currentStateCLK){
      BeatsPerMinute = (BeatsPerMinute <=  minF1) ?  minF1 : BeatsPerMinute - 1;
      frequency_flu -= 1.2;
    }
    else{
      BeatsPerMinute = (BeatsPerMinute >= maxF1) ? maxF1 : BeatsPerMinute + 1;
      frequency_flu += 1.2;
    }
    buzzer();
  }
  lastStateCLK = currentStateCLK;
  lcd.setCursor(6, 1);
  lcd.print(BeatsPerMinute);
  lcd.print(" NHIP/PHUT  ");
  general();
}
// Thiet lap gia tri min max cua khoang nhip
void handleBeatsAdjustment(){ 
  if (demSelect == 2 && menu1 == 0){
      adjustBeatsPerMinute(60, 100); // NHIP THUONG
      number = 0;
  }
  else if (demSelect == 2 && menu1 == 3){
      adjustFrequency(140, 280); // NHIP NHANH THAT
      number = 6;
  }
  else if (demSelect == 3 && menu1 == 1){
    switch (menu_bra){
    case 0:
      adjustBeatsPerMinute(25, 55); // NHIP CHAM
      number = 0;
      break;
    case 1:
      adjustBeatsPerMinute(40, 60); // NHIP BO NOI
      number = 1;
      break;
    case 2:
      adjustBeatsPerMinute(60, 90); // BLOCK NHI THAT DO I
      number = 2;
      break;
    }
  }
  else if (demSelect == 3 && menu1 == 2){
    switch (menu_sup){
    case 0:
      adjustBeatsPerMinute(100, 150); // NHIP NHANH
      number = 3;
      break;
    case 1:
      adjustBeatsPerMinute(100, 160); // NHIP NHANH NHI
      number = 4;
      break;
    case 2:
      adjustFrequency_1(80, 160); // CUONG NHI
      number = 5;
      break;   
    }
  }
}
void loop(){
  //********************************** DIEU CHINH CHON NHIP ****************************************//
  if (demSelect == 1){
    currentStateCLK = digitalRead(CLK);
    if (currentStateCLK != lastStateCLK && currentStateCLK == 1){
      if (digitalRead(DT) != currentStateCLK){
        menu1 = (menu1 <= 0) ? 3 : menu1 - 1;
        buzzer();
        menu_1();
      }
      else{
        menu1 = (menu1 >= 3) ? 0 : menu1 + 1;
        buzzer();
        menu_1();
      }
    }
    lastStateCLK = currentStateCLK;
  }
  //********************************** DIEU CHINH MENU NHIP CHAM ***********************************//
  else if (demSelect == 2 && menu1 == 1){
    currentStateCLK = digitalRead(CLK);
    if (currentStateCLK != lastStateCLK && currentStateCLK == 1){
      if (digitalRead(DT) != currentStateCLK){
        menu_bra = (menu_bra <= 0) ? 2 : menu_bra - 1;
        buzzer();
        Bradycardia();
      }
      else{
        menu_bra = (menu_bra >= 2) ? 0 : menu_bra + 1;
        buzzer();
        Bradycardia();
      }
    }
    lastStateCLK = currentStateCLK;
  }
  //********************************** DIEU CHINH MENU NHIP NHANH *********************************//
  else if (demSelect == 2 && menu1 == 2)
  {
    currentStateCLK = digitalRead(CLK);
    if (currentStateCLK != lastStateCLK && currentStateCLK == 1)
    {
      if (digitalRead(DT) != currentStateCLK)
      {
        menu_sup = (menu_sup <= 0) ? 2 : menu_sup - 1;
        buzzer();
        Supraven();
      }
      else
      {
        menu_sup = (menu_sup >= 2) ? 0 : menu_sup + 1;
        buzzer();
        Supraven();
      }
    }
    lastStateCLK = currentStateCLK;
  }

  handleBeatsAdjustment();
  
  // ********************Thiết lập nút nhấn tùy chỉnh của nút select*******************************//
  valSelect = digitalRead(select);
  if (valSelect != valSelectold){
    valSelectold = valSelect;
    if (valSelect != LOW){
      buzzer();
      demSelect++;
      demSelect = min(demSelect, 4);
      switch (demSelect){
      case 1:
        menu_1();
        break;
      case 2:
        switch (menu1){
        case 0:
          displayMenu(4, "NHIP THUONG");
          chon_menu1();
          break;
        case 1:
          Bradycardia();
          break;
        case 2:
          Supraven();
          break;
        case 3:
          displayMenu(2, "NHIP NHANH THAT");
          chon_menu1();
          break;
        }
        break;
      case 3:
        if (menu1 == 1){
          switch (menu_bra){
          case 0:
            displayMenu(2, "NHIP XOANG CHAM");
            chon_menu_bra();
            break;
          case 1:
            displayMenu(4, "NHIP BO NOI");
            chon_menu_bra();
            break;
          case 2:
            displayMenu(3, "BLOCK AV DO I");
            chon_menu_bra();
            break;
          }
        }
        else if (menu1 == 2){
          switch (menu_sup){
          case 0:
            displayMenu(2, "NHIP XOANG NHANH");
            chon_menu_sup();
            break;
          case 1:
            displayMenu(4, "NHIP NHANH NHI");
            chon_menu_sup();
            break;
          case 2:
            displayMenu(3, "CUONG NHI 4:1");
            chon_menu_sup();
            break;
          }
        }
        break;
      }
    }
  }
  //*****************************Thiet lap khi nhan nut back***************************************//
  valBack = digitalRead(back);
  if (valBack != valBackold){
    valBackold = valBack;
    if (valBack != LOW){
      tcnt2 = 131;
      TCNT2 = tcnt2;
      buzzer();
      demSelect = (demSelect <= 0) ? 0 : demSelect - 1;
      sense = 0.24;
      switch (demSelect){
      case 0:
        manhinh();
        break;
      case 1:
        menu_1();
        break;
      case 2:
        if (menu1 == 1)
          Bradycardia();
        else if (menu1 == 2)
          Supraven();
        break;
      }
    }
  }
  // Đoạn code được sử dụng để điều chỉnh độ nhạy của tín hiệu điện tâm đồ ở mức 0.5mV, 1.0mV và 2mV
  valSense = digitalRead(sens);
  if (valSense != valSenseold){
    valSenseold = valSense;
    if (valSense != LOW){
      buzzer();
      if ((demSelect == 2 && menu1 == 0) || (demSelect == 2 && menu1 == 3) || (demSelect == 3)){
        senses = (senses + 1) % 3;
        switch (senses){
        case 0:
          sense = 0.12;
          lcd.setCursor(10, 2);
          lcd.print("0.5mV");
          break;
        case 1:
          sense = 0.24;
          lcd.setCursor(10, 2);
          lcd.print("1.0mV");
          break;
        case 2:
          sense = 0.48;
          lcd.setCursor(10, 2);
          lcd.print("2.0mV");
          break;
        }
        general();
      }
    }
  }
  //Serial.println(pembagi);
}
void general(){
// Du lieu cua nhip cham va nhip thuong
  if (number == 0){
    const short y_data_template[] = {341, 341, 341, 342, 342, 343, 344, 345, 345, 346, 346, 347, 348, 349, 350, 352, 355, 357, 359, 361, 363, 364, 366, 368, 370, 373, 375, 378, 381, 386, 390, 394, 399, 404, 408, 412, 416, 421, 425, 428, 432, 435, 439, 443, 447, 451, 454, 457, 459, 462, 465, 467, 470, 471, 473, 473, 474, 474, 474, 475, 475, 474, 474, 474, 474, 474, 474, 473, 473, 472, 470, 467, 463, 460, 456, 453, 450, 446, 441, 437, 432, 427, 422, 418, 415, 411, 408, 404, 400, 396, 391, 386, 382, 378, 375, 372, 369, 367, 364, 361, 358, 355, 354, 353, 352, 352, 352, 352, 352, 352, 352, 351, 351, 350, 350, 349, 349, 349, 348, 348, 348, 348, 348, 347, 347, 347, 347, 347, 347, 347, 347, 347, 347, 347, 347, 346, 346, 346, 346, 345, 345, 344, 343, 342, 341, 339, 336, 333, 329, 325, 321, 318, 314, 309, 304, 288, 272, 256, 243, 232, 268, 305, 341, 377, 414, 450, 487, 523, 559, 596, 632, 668, 705, 741, 777, 814, 850, 887, 923, 959, 996, 1032, 1068, 1105, 1141, 1177, 1214, 1250, 1287, 1323, 1359, 1396, 1432, 1389, 1346, 1302, 1259, 1216, 1173, 1130, 1086, 1043, 1000, 957, 913, 870, 827, 784, 741, 697, 654, 611, 568, 525, 481, 438, 395, 352, 308, 265, 222, 179, 136, 92, 49, 6, 18, 29, 41, 52, 64, 75, 87, 98, 110, 121, 133, 144, 156, 167, 179, 190, 202, 213, 225, 237, 248, 260, 271, 283, 294, 306, 317, 329, 332, 335, 338, 340, 341, 342, 343, 343, 344, 345, 345, 345, 345, 345, 345, 345, 345, 344, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 346, 346, 347, 348, 350, 351, 354, 356, 358, 360, 363, 365, 367, 369, 371, 373, 375, 377, 379, 382, 384, 386, 389, 392, 394, 397, 399, 402, 405, 408, 411, 415, 418, 422, 425, 428, 431, 434, 437, 441, 445, 450, 454, 458, 462, 465, 468, 471, 475, 478, 482, 485, 488, 492, 495, 499, 502, 505, 509, 512, 515, 518, 522, 525, 528, 531, 534, 536, 539, 541, 544, 546, 548, 550, 551, 552, 553, 553, 554, 555, 557, 558, 560, 561, 562, 563, 563, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 563, 563, 562, 561, 560, 559, 557, 556, 555, 554, 553, 553, 552, 551, 550, 548, 547, 544, 542, 539, 536, 534, 531, 528, 526, 523, 520, 517, 514, 511, 507, 504, 500, 496, 492, 487, 481, 476, 471, 466, 461, 457, 452, 446, 441, 436, 431, 426, 422, 417, 413, 408, 404, 399, 393, 388, 384, 379, 376, 372, 369, 366, 364, 361, 359, 356, 354, 351, 349, 348, 346, 345, 345, 345, 344, 344, 343, 343, 342, 342, 342, 341, 341, 341, 341, 341, 341, 341, 340, 340, 339, 337, 337, 336, 334, 334, 333};
    for (int i = 0; i < 537; i++){
      data_0[i] = y_data_template[i] * sense;
    }
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);
  }
// Du lieu cua nhip bo noi
  if (number == 1){
    const short y_data_template[] = {};//{344, 344, 344, 344, 344, 343, 343, 342, 341, 341, 340, 339, 337, 335, 333, 331, 329, 327, 325, 322, 319, 314, 309, 303, 295, 286, 277, 265, 253, 240, 228, 217, 208, 202, 207, 216, 233, 255, 282, 312, 349, 391, 436, 477, 521, 566, 609, 656, 713, 768, 818, 871, 931, 982, 1035, 1090, 1140, 1183, 1234, 1282, 1325, 1371, 1409, 1435, 1449, 1451, 1436, 1417, 1390, 1349, 1303, 1250, 1186, 1118, 1053, 985, 919, 854, 794, 735, 682, 631, 577, 518, 461, 400, 336, 278, 226, 179, 139, 112, 80, 54, 33, 18, 9, 12, 22, 35, 50, 64, 78, 92, 104, 117, 130, 145, 159, 175, 190, 204, 217, 230, 241, 251, 260, 267, 275, 283, 291, 299, 306, 312, 316, 320, 323, 324, 325, 326, 327, 327, 328, 330, 331, 332, 333, 334, 334, 334, 334, 335, 335, 336, 336, 336, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 336, 336, 336, 336, 336, 336, 335, 335, 335, 335, 335, 335, 335, 335, 335, 335, 335, 335, 335, 336, 336, 337, 337, 338, 338, 339, 339, 340, 341, 341, 342, 342, 343, 343, 344, 345, 346, 348, 349, 350, 351, 353, 354, 357, 358, 360, 362, 364, 366, 368, 370, 372, 374, 377, 379, 381, 384, 386, 389, 391, 394, 397, 399, 402, 405, 408, 412, 415, 418, 421, 424, 427, 430, 434, 437, 440, 443, 446, 449, 452, 456, 459, 462, 465, 468, 471, 474, 477, 480, 483, 486, 488, 491, 494, 497, 500, 503, 506, 508, 510, 512, 514, 516, 518, 520, 521, 524, 525, 527, 529, 531, 532, 534, 535, 537, 538, 539, 540, 541, 542, 543, 544, 544, 545, 546, 547, 547, 547, 548, 548, 549, 549, 550, 550, 550, 549, 549, 548, 548, 548, 547, 547, 546, 545, 544, 543, 542, 540, 539, 538, 537, 535, 533, 531, 529, 527, 525, 522, 520, 518, 515, 513, 510, 507, 504, 501, 498, 495, 492, 488, 485, 482, 478, 475, 471, 468, 464, 460, 456, 452, 448, 445, 441, 437, 434, 430, 426, 423, 419, 416, 412, 408, 404, 400, 397, 394, 392, 389, 386, 384, 381, 378, 376, 373, 371, 368, 366, 364, 362, 360, 359, 357, 355, 354, 352, 350, 349, 349, 348, 348, 347, 347, 346, 346, 345, 345, 345, 344}; // Thay du lieu dien tam do
    for (int i = 0; i < 399; i++){
      data_1[i] = y_data_template[i] * sense;
    }
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);
  }
// Du lieu cua nhip block nhi that do I
  if (number == 2){
    const short y_data_template[] = {}; //{321, 322, 323, 324, 326, 328, 330, 333, 336, 339, 342, 346, 351, 355, 359, 362, 366, 370, 375, 379, 382, 386, 389, 393, 398, 403, 407, 411, 416, 419, 422, 425, 428, 430, 432, 435, 437, 440, 441, 443, 445, 446, 447, 448, 449, 449, 449, 449, 449, 449, 449, 449, 449, 449, 449, 448, 447, 447, 445, 443, 441, 438, 435, 433, 430, 426, 422, 419, 414, 410, 405, 400, 394, 389, 384, 380, 376, 373, 369, 366, 363, 359, 356, 352, 349, 346, 343, 340, 337, 334, 332, 329, 327, 325, 324, 322, 322, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 322, 322, 322, 322, 322, 322, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 322, 322, 322, 322, 321, 321, 321, 321, 320, 320, 319, 317, 315, 312, 309, 306, 303, 299, 294, 290, 285, 269, 254, 239, 222, 205, 200, 195, 191, 190, 228, 267, 305, 344, 382, 420, 459, 497, 536, 574, 612, 651, 689, 728, 766, 804, 843, 881, 920, 958, 996, 1035, 1073, 1112, 1150, 1188, 1227, 1265, 1304, 1342, 1305, 1268, 1232, 1195, 1158, 1121, 1084, 1047, 1011, 974, 937, 900, 863, 826, 790, 753, 716, 679, 642, 605, 569, 532, 495, 458, 421, 384, 348, 311, 274, 237, 200, 163, 127, 90, 53, 16, 25, 32, 41, 55, 68, 87, 108, 126, 141, 158, 167, 176, 185, 198, 212, 227, 242, 255, 264, 271, 276, 281, 285, 291, 296, 301, 306, 311, 315, 317, 319, 320, 320, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 322, 322, 323, 324, 326, 328, 330, 332, 334, 336, 338, 340, 341, 342, 344, 345, 346, 347, 349, 351, 354, 356, 358, 361, 363, 366, 368, 371, 373, 375, 377, 380, 383, 387, 391, 394, 398, 401, 404, 407, 410, 413, 416, 419, 423, 428, 432, 436, 440, 443, 446, 449, 452, 456, 459, 463, 467, 471, 474, 478, 482, 485, 488, 492, 496, 499, 502, 504, 507, 509, 511, 513, 515, 518, 520, 522, 524, 526, 527, 528, 529, 530, 531, 532, 534, 535, 537, 538, 539, 539, 539, 539, 539, 539, 539, 539, 539, 539, 540, 540, 540, 540, 539, 539, 539, 539, 539, 539, 539, 538, 538, 537, 535, 534, 533, 532, 531, 530, 529, 529, 528, 527, 525, 523, 521, 518, 516, 514, 512, 509, 506, 504, 501, 498, 495, 492, 489, 486, 483, 479, 474, 470, 465, 460, 456, 452, 449, 445, 442, 438, 433, 428, 422, 417, 412, 408, 404, 399, 395, 391, 386, 381, 377, 373, 369, 366, 363, 360, 357, 354, 351, 348, 345, 342, 339, 336, 333, 330, 328, 325, 324, 322, 321, 321};
    for (int i = 0; i < 662; i++){
      data_2[i] = y_data_template[i] * sense;
    }
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);
  }
// Du lieu cua nhip nhanh
  if (number == 3){
    const short y_data_template[] = {};//{315, 316, 316, 316, 317, 317, 317, 318, 318, 319, 320, 321, 322, 324, 325, 326, 327, 328, 330, 332, 333, 336, 338, 340, 343, 345, 348, 350, 352, 355, 358, 360, 363, 366, 369, 371, 374, 377, 379, 382, 385, 388, 390, 393, 395, 397, 399, 402, 404, 406, 407, 409, 410, 411, 411, 412, 413, 413, 413, 414, 414, 414, 414, 414, 414, 414, 414, 413, 413, 413, 412, 410, 409, 407, 405, 403, 401, 397, 394, 391, 388, 385, 381, 378, 374, 370, 366, 363, 359, 355, 352, 348, 345, 342, 339, 336, 333, 330, 327, 324, 322, 320, 318, 316, 315, 313, 311, 309, 308, 307, 307, 307, 307, 307, 307, 307, 307, 307, 307, 306, 305, 304, 304, 303, 303, 302, 302, 301, 299, 297, 295, 291, 287, 283, 278, 274, 269, 263, 258, 250, 243, 233, 225, 215, 204, 192, 180, 167, 156, 152, 156, 179, 208, 250, 300, 347, 399, 451, 514, 580, 651, 734, 818, 897, 989, 1075, 1143, 1209, 1254, 1267, 1249, 1213, 1170, 1100, 1028, 957, 876, 791, 686, 584, 477, 383, 280, 220, 145, 99, 59, 37, 26, 37, 52, 67, 82, 97, 111, 125, 139, 152, 165, 176, 186, 200, 212, 224, 238, 252, 261, 271, 281, 290, 297, 304, 309, 313, 315, 316, 316, 317, 317, 317, 317, 317, 317, 317, 317, 317, 317, 317, 317, 317, 317, 317, 318, 318, 319, 321, 322, 324, 326, 328, 331, 333, 336, 339, 342, 345, 348, 352, 355, 359, 362, 366, 370, 374, 378, 382, 386, 390, 394, 398, 402, 407, 411, 414, 417, 421, 424, 427, 430, 434, 437, 440, 442, 444, 447, 449, 451, 453, 456, 458, 459, 461, 463, 465, 467, 468, 470, 471, 472, 473, 473, 474, 475, 475, 476, 477, 477, 478, 479, 479, 479, 479, 479, 479, 479, 479, 479, 478, 477, 477, 476, 476, 475, 474, 474, 473, 471, 470, 469, 467, 465, 463, 461, 459, 458, 456, 454, 452, 449, 447, 444, 442, 440, 437, 434, 431, 429, 426, 423, 420, 417, 413, 410, 406, 402, 399, 395, 391, 387, 383, 379, 375, 371, 367, 363, 359, 356, 352, 349, 345, 342, 338, 335, 332, 329, 326, 324, 322, 320, 318, 317, 316, 316, 315, 315, 314, 314, 313, 313, 313};
    for (int i = 0; i < 380; i++){
      data_3[i] = y_data_template[i] * sense;
    }
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);
  }
// Du lieu cua nhip nhanh nhi
  if (number == 4){
    const short y_data_template[] = {};//{203, 203, 203, 202, 201, 200, 199, 198, 196, 194, 191, 189, 186, 183, 180, 175, 170, 165, 160, 155, 149, 144, 138, 131, 124, 117, 110, 104, 97, 92, 86, 81, 75, 69, 63, 58, 55, 55, 58, 64, 72, 79, 86, 93, 100, 105, 112, 120, 127, 134, 143, 151, 159, 168, 176, 181, 187, 191, 194, 197, 199, 201, 202, 203, 203, 204, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 204, 204, 204, 204, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 204, 204, 204, 203, 201, 199, 196, 193, 189, 186, 183, 179, 176, 173, 169, 168, 212, 256, 300, 344, 388, 433, 477, 521, 565, 609, 653, 697, 741, 785, 829, 873, 917, 962, 1006, 1050, 1094, 1138, 1182, 1139, 1096, 1053, 1010, 968, 925, 882, 839, 796, 753, 710, 667, 624, 582, 539, 496, 453, 410, 367, 324, 281, 238, 196, 153, 110, 67, 24, 27, 35, 49, 60, 73, 85, 97, 107, 119, 129, 138, 148, 158, 165, 176, 186, 194, 199, 203, 204, 204, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 204, 204, 204, 204, 204, 204, 204, 205, 206, 207, 209, 210, 211, 213, 214, 215, 217, 219, 221, 223, 226, 228, 230, 233, 235, 238, 240, 243, 246, 248, 251, 254, 257, 259, 262, 265, 268, 271, 274, 278, 281, 285, 289, 292, 296, 300, 304, 308, 314, 318, 323, 328, 333, 337, 342, 347, 353, 359, 366, 372, 378, 384, 390, 396, 402, 408, 414, 420, 426, 432, 437, 443, 449, 454, 459, 465, 470, 475, 481, 487, 492, 497, 501, 505, 509, 513, 517, 521, 524, 527, 530, 532, 535, 536, 538, 539, 540, 540, 541, 540, 539, 538, 535, 532, 526, 520, 512, 503, 493, 484, 473, 464, 454, 444, 433, 423, 412, 401, 387, 374, 361, 351, 339, 330, 321, 311, 301, 293, 286, 280, 275, 270, 265, 262, 258, 254, 251, 248, 245, 242, 240, 238, 235, 233, 231, 229, 227, 225, 223, 221, 219, 217, 216, 214, 213, 212, 210, 209, 207, 206, 205, 204, 204, 204, 203};
    for (int i = 0; i < 363; i++){
      data_4[i] = y_data_template[i] * sense;
    }
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);
  }
// Du lieu cua nhip cuong nhi
  if (number == 5){
    const short y_data_template[] = {};//{223, 233, 243, 251, 258, 264, 269, 276, 282, 289, 299, 309, 318, 327, 335, 341, 345, 350, 355, 359, 361, 363, 362, 360, 356, 352, 348, 343, 337, 332, 326, 321, 317, 313, 309, 304, 299, 293, 287, 281, 275, 269, 263, 257, 251, 245, 239, 232, 227, 221, 216, 210, 205, 200, 194, 189, 185, 181, 178, 174, 171, 168, 165, 162, 160, 158, 156, 155, 153, 151, 150, 149, 147, 146, 145, 143, 142, 141, 139, 138, 136, 134, 131, 128, 126, 123, 118, 113, 107, 100, 94, 90, 86, 83, 81, 79, 78, 77, 76, 75, 74, 73, 73, 72, 72, 71, 72, 73, 76, 79, 84, 89, 94, 100, 107, 115, 123, 132, 142, 152, 161, 171, 181, 191, 201, 211, 220, 229, 238, 247, 257, 267, 278, 289, 299, 309, 318, 327, 334, 342, 348, 355, 362, 368, 375, 381, 387, 393, 398, 403, 407, 411, 413, 414, 415, 413, 412, 409, 406, 402, 398, 394, 389, 383, 377, 371, 365, 359, 353, 347, 340, 333, 325, 318, 310, 302, 294, 286, 278, 269, 260, 245, 229, 214, 199, 184, 175, 171, 171, 224, 278, 331, 384, 438, 491, 544, 597, 651, 704, 757, 811, 864, 917, 971, 1024, 1077, 1130, 1184, 1237, 1186, 1135, 1084, 1032, 981, 930, 879, 828, 777, 725, 674, 623, 572, 521, 470, 418, 367, 316, 265, 214, 163, 111, 60, 9, 10, 12, 19, 26, 32, 38, 43, 42, 40, 37, 34, 30, 27, 23, 20, 16, 12, 8, 5, 4, 2, 2, 2, 4, 6, 10, 14, 18, 23, 28, 33, 39, 46, 53, 60, 67, 74, 82, 90, 99, 109, 121, 132, 144, 155, 168, 180, 190, 201, 212, 222, 233, 245, 258, 272, 286, 298, 308, 319, 327, 336, 343, 350, 355, 357, 357, 355, 352, 348, 343, 336, 329, 322, 314, 306, 298, 291, 284, 278, 271, 265, 259, 252, 246, 240, 233, 227, 221, 215, 209, 203, 198, 192, 187, 182, 177, 172, 168, 165, 161, 159, 156, 154, 152, 149, 147, 145, 143, 140, 138, 136, 134, 131, 128, 124, 120, 115, 110, 104, 99, 95, 91, 88, 87, 85, 83, 82, 81, 79, 78, 77, 76, 76, 76, 77, 78, 80, 82, 85, 88, 93, 97, 103, 109, 116, 124, 131, 137, 143, 149, 153, 158, 163, 167, 171, 175, 178, 181, 185, 188, 191, 194, 197, 200, 204, 208, 212, 217, 221, 226, 230, 235, 239, 244, 248, 251, 255, 259, 262, 266, 270, 275, 280, 287, 294, 301, 308, 314, 320, 325, 328, 332, 335, 337, 339, 339, 338, 336, 334, 330, 326, 322, 318, 313, 308, 304, 299, 295, 292, 287, 283, 278, 272, 265, 258, 251, 245, 241, 236, 231, 225, 219, 213, 207, 201, 197, 193, 189, 186, 182, 179, 176, 173, 170, 167, 164, 162, 159, 157, 154, 152, 150, 148, 146, 144, 142, 140, 137, 135, 132, 128, 124, 119, 113, 107, 101, 96, 91, 89, 87, 85, 83, 82, 80, 79, 78, 77, 76, 75, 74, 73, 73, 72, 71, 70, 70, 70, 70, 70, 71, 72, 73, 75, 77, 80, 85, 90, 96, 101, 107, 114, 122, 131, 141, 151, 162, 172, 182, 191, 200, 209, 217, 225};
    for (int i = 0; i < 536; i++){
      data_5[i] = y_data_template[i] * sense;
    }
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);
  }
// Du lieu cua nhip nhanh that
  if (number == 6){
    const short y_data_template[] = {1, 3, 8, 14, 30, 46, 62, 76, 93, 100, 108, 117, 128, 136, 150, 162, 180, 195, 211, 222, 239, 250, 264, 277, 296, 309, 323, 335, 350, 360, 372, 384, 400, 412, 425, 435, 448, 456, 465, 474, 488, 500, 516, 531, 545, 554, 568, 578, 593, 609, 624, 633, 666, 693, 728, 762, 804, 823, 846, 862, 884, 898, 915, 928, 942, 952, 973, 990, 1015, 1039, 1069, 1091, 1119, 1139, 1168, 1189, 1212, 1228, 1246, 1256, 1267, 1274, 1283, 1289, 1297, 1304, 1314, 1322, 1331, 1337, 1345, 1351, 1358, 1366, 1374, 1380, 1386, 1392, 1398, 1406, 1412, 1419, 1427, 1433, 1439, 1445, 1451, 1456, 1463, 1469, 1476, 1484, 1492, 1497, 1504, 1509, 1514, 1518, 1523, 1526, 1530, 1534, 1541, 1548, 1555, 1562, 1569, 1573, 1577, 1580, 1582, 1584, 1588, 1592, 1597, 1601, 1605, 1607, 1608, 1609, 1609, 1610, 1610, 1609, 1608, 1607, 1604, 1600, 1597, 1593, 1590, 1587, 1583, 1578, 1573, 1567, 1562, 1558, 1552, 1547, 1541, 1535, 1527, 1519, 1510, 1500, 1486, 1474, 1457, 1444, 1430, 1420, 1409, 1402, 1390, 1378, 1362, 1350, 1337, 1329, 1322, 1319, 1313, 1307, 1293, 1278, 1263, 1247, 1232, 1225, 1220, 1216, 1214, 1212, 1211, 1210, 1217, 1225, 1232, 1241, 1249, 1251, 1261, 1271, 1288, 1305, 1321, 1332, 1342, 1346, 1351, 1355, 1366, 1377, 1392, 1406, 1420, 1426, 1427, 1424, 1393, 1361, 1326, 1294, 1250, 1231, 1207, 1186, 1147, 1119, 1079, 1043, 1000, 975, 926, 890, 833, 784, 711, 663, 590, 539, 477, 438, 390, 365, 342, 329, 293, 266, 225, 183, 141, 123, 89, 70, 52, 34, 17, 17};
    for (int i = 0; i < 260; i++){
      data_6[i] = y_data_template[i] * sense;
    }
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);
  }
}
ISR(TIMER2_OVF_vect)
{
  TCNT2 = tcnt2;
  // Thiet lap truyen du lieu cua nhip thuong va nhip cham
  if (number == 0)
  {
    switch (State)
    {
    case INIT:
      QRSCount = 0;
      IdleCount = 0;
      State = QRS;
      break;
    case QRS:
      DTOA_Send(data_0[QRSCount]);
      QRSCount++;
      if (QRSCount >= NumSample)
      {
        QRSCount = 0;
        DTOA_Send(data_0[0]);
        State = IDLE;
      }
      break;
    case IDLE:
      IdleCount++;
      if (IdleCount >= IdlePeriod)
      {
        IdleCount = 0;
        State = QRS;
      }
      break;
    default:
      break;
    }
  }
  // Thiet lap truyen du lieu cua nhip bo noi
  if (number == 1)
  {
    switch (State)
    {
    case INIT:
      QRSCount = 0;
      IdleCount = 0;
      State = QRS;
      break;
    case QRS:
      DTOA_Send(data_1[QRSCount]);
      QRSCount++;
      if (QRSCount >= NumSample)
      {
        QRSCount = 0;
        DTOA_Send(data_1[0]);
        State = IDLE;
      }
      break;
    case IDLE:
      IdleCount++;
      if (IdleCount >= IdlePeriod)
      {
        IdleCount = 0;
        State = QRS;
      }
      break;
    default:
      break;
    }
  }
  // Thiet lap truyen du lieu cua nhip block nhi that do I
  if (number == 2)
  {
    switch (State)
    {
    case INIT:
      QRSCount = 0;
      IdleCount = 0;
      State = QRS;
      break;
    case QRS:
      DTOA_Send(data_2[QRSCount]);
      QRSCount++;
      if (QRSCount >= NumSample)
      {
        QRSCount = 0;
        DTOA_Send(data_2[0]);
        State = IDLE;
      }
      break;
    case IDLE:
      IdleCount++;
      if (IdleCount >= IdlePeriod)
      {
        IdleCount = 0;
        State = QRS;
      }
      break;
    default:
      break;
    }
  }
  // Thiet lap truyen du lieu cua nhip nhanh
  if (number == 3)
  {
    switch (State)
    {
    case INIT:
      QRSCount = 0;
      IdleCount = 0;
      State = QRS;
      break;
    case QRS:
      DTOA_Send(data_3[QRSCount]);
      QRSCount++;
      if (QRSCount >= NumSample)
      {
        QRSCount = 0;
        DTOA_Send(data_3[0]);
        State = IDLE;
      }
      break;
    case IDLE:
      IdleCount++;
      if (IdleCount >= IdlePeriod)
      {
        IdleCount = 0;
        State = QRS;
      }
      break;
    default:
      break;
    }
  }
  // Thiet lap truyen du lieu cua nhip nhanh nhi
  if (number == 4)
  {
    switch (State)
    {
    case INIT:
      QRSCount = 0;
      IdleCount = 0;
      State = QRS;
      break;
    case QRS:
      DTOA_Send(data_4[QRSCount]);
      QRSCount++;
      if (QRSCount >= NumSample)
      {
        QRSCount = 0;
        DTOA_Send(data_4[0]);
        State = IDLE;
      }
      break;
    case IDLE:
      IdleCount++;
      if (IdleCount >= IdlePeriod)
      {
        IdleCount = 0;
        State = QRS;
      }
      break;
    default:
      break;
    }
  }
  // Thiet lap truyen du lieu cua cuong nhi
  if (number == 5)
  {
      tcnt2 = 256 - (F_CPU / 1024 / frequency_flu);
      TCNT2 = tcnt2;
    switch (State)
    {
    case INIT:
      QRSCount = 0;
      IdleCount = 0;
      State = QRS;
      break;
    case QRS:
      DTOA_Send(data_5[QRSCount]);
      QRSCount++;
      if (QRSCount >= NumSample)
      {
        QRSCount = 0;
        DTOA_Send(data_5[0]);
        State = IDLE;
      }
      break;
    case IDLE:
//      IdleCount++;
//      if (IdleCount >= IdlePeriod)
//      {
//        IdleCount = 0;
        State = QRS;
//      }
      break;
    default:
      break;
    }
  }
  // Thiet lap truyen du lieu cua nhip nhanh that
  if (number == 6)
  {
      tcnt2 = 256 - (F_CPU / 1024 / frequency_ven);
      TCNT2 = tcnt2;
    switch (State)
    {
    case INIT:
      QRSCount = 0;
      IdleCount = 0;
      State = QRS;
      break;
    case QRS:
      DTOA_Send(data_6[QRSCount]);
      QRSCount++;
      if (QRSCount >= NumSample)
      {
        QRSCount = 0;
        DTOA_Send(data_6[0]);
        State = IDLE;
      }
      break;
    case IDLE:
//      IdleCount++;
//      if (IdleCount >= IdlePeriod)
//      {
//        IdleCount = 0;
        State = QRS;
//      }
      break;
    default:
      break;
    }
  }
}

void DTOA_Send(unsigned short DtoAValue){
  byte Data = 0;
  digitalWrite(53, 0);
  Data = highByte(DtoAValue);
  Data = 0b00001111 & Data;
  Data = 0b00110000 | Data;
  SPI.transfer(Data);
  Data = lowByte(DtoAValue);
  SPI.transfer(Data);
  digitalWrite(53, 1);
}
void buzzer(){
  digitalWrite(buz, HIGH);
  delay(50);
  digitalWrite(buz, LOW);
}
