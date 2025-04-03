// Library includes
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Define signal phase states
#define INIT 0
#define IDLE 1
#define QRS 2

// Define constants for waveform patterns
#define FOUR 4
#define THREE 3
#define TWO 2
#define ONE 1

// Encoder pin definitions
const int CLK = 23;
const int DT = 25;
int currentStateCLK;
int lastStateCLK;

// Frequency control variables for fast rhythms
float frequency_flu = 67.98;
float frequency_ven = 76.98;

// Other pin definitions
const int buz = 24;
const int select = 27;
const int back = 29;
const int sens = 31;

// Button state variables
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

// Default heart rate values for ECG signal types
int Bpm_select = 0;
int Bpm_normal = 75;          // Default heart rate for normal rhythm (60–100 bpm)

int Bpm_slow = 50;            // Default heart rate for slow rhythm (25–55 bpm)
int Bpm_junc = 50;            // Default heart rate for junctional rhythm (40–60 bpm)
int Bpm_block = 65;           // Default heart rate for first-degree AV block (60–100 bpm)

int Bpm_sinus = 100;          // Default heart rate for sinus tachycardia (100–150 bpm)
int Bpm_atrial = 100;         // Default heart rate for atrial tachycardia (100–150 bpm)
int Bpm_flutter = 60;         // Default heart rate for atrial flutter (60–110 bpm)

int Bpm_ven = 140;            // Default heart rate for ventricular tachycardia (140–240 bpm)

// Global timing variables
unsigned int QRSCount = 0;    // Duration of QRS phase (ms)
unsigned int IdleCount = 0;   // Duration of Idle phase (ms)
unsigned long IdlePeriod = 0; // Idle period adjusted to set the heart rate
unsigned int State = INIT;    // System state: INIT, QRS, or IDLE
float tcnt2;           // Timer2 reload value

// Variables for heart rate adjustment
int BeatsPerMinute = 0;       // BPM value calculated during NumSample computation
int pembagi = 0;              // Divider ratio for sampling rate
float sense = 0.24;           // Default sensitivity value
unsigned int NumSample;       // Sample size calculated by formula (size / (2 + pembagi/100))
int number = -1;              // Initial value used for rhythm case selection

short data_0[403] = {}; // Declare data array for normal rhythm
//short data_1[300] = {}; // Declare data array for junctional rhythm
short data_1[0] = {};
//short data_2[497] = {}; // Declare data array for first-degree AV block
short data_2[0] = {};
short data_3[380] = {}; // Declare data array for sinus tachycardia
//short data_3[0] = {};
//short data_4[363] = {}; // Declare data array for focal atrial tachycardia
short data_4[0] = {};
short data_5[536] = {}; // Declare data array for atrial fibrillation 4:1
short data_6[260] = {}; // Declare data array for ventricular tachycardia


LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup() {
  Serial.begin(115200);
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  // Configure button pins
  pinMode(select, INPUT_PULLUP);
  pinMode(back, INPUT_PULLUP);
  pinMode(sens, INPUT_PULLUP);
  pinMode(buz, OUTPUT);

  // Configure SPI pins
  pinMode(53, OUTPUT);
  pinMode(51, OUTPUT);
  pinMode(52, OUTPUT);

  // Configure encoder pins
  pinMode(CLK, INPUT);
  pinMode(DT, INPUT);
  lastStateCLK = digitalRead(CLK);

  // Display startup interface
  lcd.clear();
  lcd.setCursor(3, 1);
  lcd.print("DESIGN BY VHT");
  delay(2000);
  lcd.clear();

  // Create custom character ">" for menu selection
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

  // Initialize SPI interface
  SPI.begin();
  SPI.setDataMode(0);
  SPI.setClockDivider(SPI_CLOCK_DIV64);
  SPI.setBitOrder(MSBFIRST);

  // Configure Timer2 for interrupt
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

  // Display main screen
  manhinh();
}

// Setup main display screen
void manhinh() {
  lcd.clear();
  lcd.setCursor(3, 1);
  lcd.print("ECG SIMULATOR");
}

// Setup interface for selecting rhythm group
void menu_1() {
  lcd.clear();
  lcd.setCursor(0, 0);
  const char *options[] = {" NORMAL", " BRADYCARDIA", " TACHYCARDIA", " VENTRICULAR"};
  const int displayCount = 4;
  int startIdx = max(0, menu1 - displayCount + 1);
  if (menu1 >= startIdx + displayCount) {
    startIdx = menu1 - displayCount + 1;
  }
  for (int i = 0; i < displayCount; ++i) {
    int optionIdx = startIdx + i;
    if (optionIdx >= 0 && optionIdx < sizeof(options) / sizeof(options[0])) {
      lcd.setCursor(i == menu1 - startIdx ? 0 : 1, i);
      if (optionIdx == menu1) {
        lcd.write((uint8_t)0); // Write custom ">" arrow character
      }
      lcd.print(options[optionIdx]);
    }
  }
}

// Setup interface for bradycardia rhythms
void Bradycardia() {
  lcd.clear();
  lcd.setCursor(0, 0);
  const char *options[] = {" SINUS BRADYCARDIA", " JUNCTIONAL RHYTHM", " I-AV BLOCK"};
  const int displayCount = 3;
  int startIdx = max(0, menu_bra - displayCount + 1);
  if (menu_bra >= startIdx + displayCount) {
    startIdx = menu_bra - displayCount + 1;
  }
  for (int i = 0; i < displayCount; ++i) {
    int optionIdx = startIdx + i;
    if (optionIdx >= 0 && optionIdx < sizeof(options) / sizeof(options[0])) {
      lcd.setCursor(i == menu_bra - startIdx ? 0 : 1, i);
      if (optionIdx == menu_bra) {
        lcd.write((uint8_t)0); // Write custom ">" arrow character
      }
      lcd.print(options[optionIdx]);
    }
  }
}

// Setup interface for tachycardia rhythms
void Supraven() {
  lcd.clear();
  lcd.setCursor(0, 0);
  const char *options[] = {" SINUS TACHYCARDIA", " FOCAL A-TACHY", " AFib 4:1"};
  const int displayCount = 3;
  int startIdx = max(0, menu_sup - displayCount + 1);
  if (menu_sup >= startIdx + displayCount) {
    startIdx = menu_sup - displayCount + 1;
  }
  for (int i = 0; i < displayCount; ++i) {
    int optionIdx = startIdx + i;
    if (optionIdx >= 0 && optionIdx < sizeof(options) / sizeof(options[0])) {
      lcd.setCursor(i == menu_sup - startIdx ? 0 : 1, i);
      if (optionIdx == menu_sup) {
        lcd.write((uint8_t)0); // Write custom ">" arrow character
      }
      lcd.print(options[optionIdx]);
    }
  }
}

// Set BPM and other related values
void set_beats_per_minute(int menu_value, int bpm_values[], int pembagi_values[], int num_values) {
  if (menu_value >= 0 && menu_value < num_values) {
    BeatsPerMinute = bpm_values[menu_value];
    pembagi = pembagi_values[menu_value];
  }
}

// Set selection for normal rhythm (menu 1)
void chon_menu1() {
  int bpm_values[] = {Bpm_normal, 0, 0, Bpm_ven};
  int pembagi_values[] = {0, 0, 0, 70};
  set_beats_per_minute(menu1, bpm_values, pembagi_values, 4);
}

// Set selection for bradycardia rhythms (menu bra)
void chon_menu_bra() {
  int bpm_values[] = {Bpm_slow, Bpm_junc, Bpm_block};
  int pembagi_values[] = {0, 0, 0};
  set_beats_per_minute(menu_bra, bpm_values, pembagi_values, 3);
}

// Set selection for supraventricular rhythms (menu sup)
void chon_menu_sup() {
  int bpm_values[] = {Bpm_sinus, Bpm_atrial, Bpm_flutter};
  int pembagi_values[] = {0, 0, 20};
  set_beats_per_minute(menu_sup, bpm_values, pembagi_values, 3);
}

// Setup display interface and heart rate adjustment
void displayMenu(int titleX, String title) {
  lcd.clear();
  lcd.setCursor(titleX, 0);
  lcd.print(title);
  lcd.setCursor(0, 1);
  lcd.print("HR = ");
  lcd.setCursor(0, 2);
  lcd.print("SENSITIVITY = 1.0mV");
}

// Setup encoder to adjust heart rate (BPM)
void adjustBeatsPerMinute(int minBPM, int maxBPM) {
  // Temporarily disable interrupts to safely update timing
  noInterrupts();
  // Calculate idle period based on BPM and number of waveform samples
  IdlePeriod = (unsigned int)((float)60000.0 / BeatsPerMinute) - (float)NumSample;
  interrupts();  // Re-enable interrupts
  // Read current state of the CLK pin (rotary encoder)
  currentStateCLK = digitalRead(CLK);
  // Check if the encoder has been rotated
  if (currentStateCLK != lastStateCLK && currentStateCLK == 1) {
    // Determine rotation direction based on DT pin
    if (digitalRead(DT) != currentStateCLK) {
      // Rotate left: decrease BPM but not below minimum
      BeatsPerMinute = (BeatsPerMinute <= minBPM) ? minBPM : BeatsPerMinute - 1;
    } else {
      // Rotate right: increase BPM but not above maximum
      BeatsPerMinute = (BeatsPerMinute >= maxBPM) ? maxBPM : BeatsPerMinute + 1;
    }
    // Trigger buzzer to provide user feedback on change
    buzzer();
  }
  // Save the current state of the encoder for next comparison
  lastStateCLK = currentStateCLK;
  // Display updated BPM on LCD
  lcd.setCursor(6, 1);
  lcd.print(BeatsPerMinute);
  lcd.print(" BPM  ");
  // Call general() to apply updated BPM or other display updates
  general();
}

// Setup fast rhythm adjustment without pause interval
void adjustFrequency(int minF, int maxF) {
  // Read encoder CLK pin
  currentStateCLK = digitalRead(CLK);

  // Check if encoder has been rotated
  if (currentStateCLK != lastStateCLK && currentStateCLK == 1) {
    // Determine direction and adjust BPM accordingly
    if (digitalRead(DT) != currentStateCLK) {
      BeatsPerMinute = (BeatsPerMinute <= minF) ? minF : BeatsPerMinute - 1;
    } else {
      BeatsPerMinute = (BeatsPerMinute >= maxF) ? maxF : BeatsPerMinute + 1;
    }

    // Convert BPM to frequency value (specific for ventricular waveform)
    frequency_ven = (BeatsPerMinute - 0.6287) / 1.8104;
    tcnt2 = 256 - (F_CPU / 1024 / frequency_ven);
    // Provide buzzer feedback
    buzzer();
  }

  lastStateCLK = currentStateCLK;

  // Update BPM display on LCD
  lcd.setCursor(6, 1);
  lcd.print(BeatsPerMinute);
  lcd.print(" BPM  ");

  // Call general update function
  general();
}

// Setup alternate fast rhythm adjustment without pause interval
void adjustFrequency_1(int minF1, int maxF1) {

  // Read encoder CLK pin
  currentStateCLK = digitalRead(CLK);

  // Check if encoder has been rotated
  if (currentStateCLK != lastStateCLK && currentStateCLK == 1) {
    // Determine direction and adjust BPM accordingly
    if (digitalRead(DT) != currentStateCLK) {
      BeatsPerMinute = (BeatsPerMinute <= minF1) ? minF1 : BeatsPerMinute - 1;
    } else {
      BeatsPerMinute = (BeatsPerMinute >= maxF1) ? maxF1 : BeatsPerMinute + 1;
    }
    // Provide buzzer feedback
    buzzer();
    // Convert BPM to frequency value (specific for flutter waveform)
    frequency_flu = (BeatsPerMinute - 0.1467)/0.8805;
    tcnt2 = 256 - (F_CPU / 1024 / frequency_flu);
  }

  lastStateCLK = currentStateCLK;

  // Update BPM display on LCD
  lcd.setCursor(6, 1);
  lcd.print(BeatsPerMinute);
  lcd.print(" BPM  ");

  // Call general update function
  general();
}

// Set min and max values for heart rate adjustment range
void handleBeatsAdjustment() { 
  if (demSelect == 2 && menu1 == 0) {
    adjustBeatsPerMinute(60, 100); // Normal rhythm
    number = 0;
  }
  else if (demSelect == 2 && menu1 == 3) {
    adjustFrequency(140, 240); // Ventricular tachycardia
    number = 6;
  }
  else if (demSelect == 3 && menu1 == 1) {
    switch (menu_bra) {
      case 0:
        adjustBeatsPerMinute(25, 55); // Sinus bradycardia
        number = 0;
        break;
      case 1:
        adjustBeatsPerMinute(40, 60); // Junctional rhythm
        number = 1;
        break;
      case 2:
        adjustBeatsPerMinute(60, 90); // First-degree AV block
        number = 2;
        break;
    }
  }
  else if (demSelect == 3 && menu1 == 2) {
    switch (menu_sup) {
      case 0:
        adjustBeatsPerMinute(100, 150); // Sinus tachycardia
        number = 3;
        break;
      case 1:
        adjustBeatsPerMinute(100, 150); // Atrial tachycardia focal
        number = 4;
        break;
      case 2:
        adjustFrequency_1(60, 110); // Atrial flutter
        number = 5;
        break;   
    }
  }
}

void loop() {
  //Serial.println(tcnt2);
    // Adjust main rhythm group selection
  if (demSelect == 1) {
    currentStateCLK = digitalRead(CLK);
    if (currentStateCLK != lastStateCLK && currentStateCLK == 1) {
      if (digitalRead(DT) != currentStateCLK) {
        menu1 = (menu1 <= 0) ? 3 : menu1 - 1;
        buzzer();
        menu_1();
      } else {
        menu1 = (menu1 >= 3) ? 0 : menu1 + 1;
        buzzer();
        menu_1();
      }
    }
    lastStateCLK = currentStateCLK;
  }

  // Adjust sub-menu for bradycardia rhythms
  else if (demSelect == 2 && menu1 == 1) {
    currentStateCLK = digitalRead(CLK);
    if (currentStateCLK != lastStateCLK && currentStateCLK == 1) {
      if (digitalRead(DT) != currentStateCLK) {
        menu_bra = (menu_bra <= 0) ? 2 : menu_bra - 1;
        buzzer();
        Bradycardia();
      } else {
        menu_bra = (menu_bra >= 2) ? 0 : menu_bra + 1;
        buzzer();
        Bradycardia();
      }
    }
    lastStateCLK = currentStateCLK;
  }

  // Adjust sub-menu for supraventricular rhythms
  else if (demSelect == 2 && menu1 == 2) {
    currentStateCLK = digitalRead(CLK);
    if (currentStateCLK != lastStateCLK && currentStateCLK == 1) {
      if (digitalRead(DT) != currentStateCLK) {
        menu_sup = (menu_sup <= 0) ? 2 : menu_sup - 1;
        buzzer();
        Supraven();
      } else {
        menu_sup = (menu_sup >= 2) ? 0 : menu_sup + 1;
        buzzer();
        Supraven();
      }
    }
    lastStateCLK = currentStateCLK;
  }

  handleBeatsAdjustment();

  // Handle select button press for menu navigation
  valSelect = digitalRead(select);
  if (valSelect != valSelectold) {
    valSelectold = valSelect;
    if (valSelect != LOW) {
      buzzer();
      demSelect++;
      demSelect = min(demSelect, 4);
      switch (demSelect) {
        case 1:
          menu_1();
          break;
        case 2:
          switch (menu1) {
            case 0:
              displayMenu(7, "NORMAL");
              chon_menu1();
              break;
            case 1:
              Bradycardia();
              break;
            case 2:
              Supraven();
              break;
            case 3:
              displayMenu(4, "VENTRICULAR");
              chon_menu1();
              tcnt2 = 53.03;
              break;
          }
          break;
        case 3:
          if (menu1 == 1) {
            switch (menu_bra) {
              case 0:
                displayMenu(1, "SINUS BRADYCARDIA");
                chon_menu_bra();
                break;
              case 1:
                displayMenu(2, "JUNCTIONAL RHYTHM");
                chon_menu_bra();
                break;
              case 2:
                displayMenu(5, "I-AV BLOCK");
                chon_menu_bra();
                break;
            }
          } else if (menu1 == 2) {
            switch (menu_sup) {
              case 0:
                displayMenu(2, "SINUS TACHYCARDIA");
                chon_menu_sup();
                break;
              case 1:
                displayMenu(3, "FOCAL A-TACHY");
                chon_menu_sup();
                break;
              case 2:
                displayMenu(6, "AFIB 4:1");
                chon_menu_sup();
                tcnt2 = 26.14;
                break;
            }
          }
          break;
      }
    }
  }

  // Handle back button press to return to previous menu
  valBack = digitalRead(back);
  if (valBack != valBackold) {
    valBackold = valBack;
    if (valBack != LOW) {
      tcnt2 = 131;
      TCNT2 = tcnt2;
      buzzer();
      demSelect = (demSelect <= 0) ? 0 : demSelect - 1;
      sense = 0.24;
      switch (demSelect) {
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

  // Handle sensitivity adjustment: 0.5mV, 1.0mV, 2.0mV
  valSense = digitalRead(sens);
  if (valSense != valSenseold) {
    valSenseold = valSense;
    if (valSense != LOW) {
      buzzer();
      if ((demSelect == 2 && menu1 == 0) || (demSelect == 2 && menu1 == 3) || (demSelect == 3)) {
        senses = (senses + 1) % 3;
        switch (senses) {
          case 0:
            sense = 0.12;
            lcd.setCursor(14, 2);
            lcd.print("0.5mV");
            break;
          case 1:
            sense = 0.24;
            lcd.setCursor(14, 2);
            lcd.print("1.0mV");
            break;
          case 2:
            sense = 0.48;
            lcd.setCursor(14, 2);
            lcd.print("2.0mV");
            break;
        }
        general();
      }
    }
  }
}

void general(){
// Bradycardia and normal rhythm data
  if (number == 0){
    const short y_data_template[] = {341, 341, 341, 342, 343, 344, 345, 346, 346, 348, 349, 350, 355, 357, 359, 363, 364, 366, 370, 373, 375, 381, 386, 390, 399, 404, 408, 416, 421, 425, 432, 435, 439, 447, 451, 454, 459, 462, 465, 470, 471, 473, 474, 474, 474, 475, 474, 474, 474, 474, 474, 473, 472, 470, 463, 460, 456, 450, 446, 441, 432, 427, 422, 415, 411, 408, 400, 396, 391, 382, 378, 375, 369, 367, 364, 358, 355, 354, 352, 352, 352, 352, 352, 352, 351, 350, 350, 349, 349, 348, 348, 348, 348, 347, 347, 347, 347, 347, 347, 347, 347, 347, 346, 346, 346, 345, 344, 343, 341, 339, 336, 329, 325, 321, 314, 309, 304, 272, 256, 243, 263, 295, 327, 390, 422, 453, 517, 548, 580, 643, 675, 707, 770, 802, 833, 897, 928, 960, 1023, 1055, 1087, 1150, 1182, 1213, 1277, 1238, 1200, 1123, 1084, 1046, 969, 930, 892, 815, 776, 738, 661, 622, 584, 507, 468, 430, 353, 314, 276, 199, 160, 122, 45, 6, 18, 41, 52, 64, 87, 98, 110, 133, 144, 156, 179, 190, 202, 225, 237, 248, 271, 283, 294, 317, 329, 332, 338, 340, 341, 343, 343, 344, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 345, 346, 347, 348, 351, 354, 356, 360, 363, 365, 369, 371, 373, 377, 379, 382, 386, 389, 392, 397, 399, 402, 408, 411, 415, 422, 425, 428, 434, 437, 441, 450, 454, 458, 465, 468, 471, 478, 482, 485, 492, 495, 499, 505, 509, 512, 518, 522, 525, 531, 534, 536, 541, 544, 546, 550, 551, 552, 553, 554, 555, 558, 560, 561, 563, 563, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 564, 563, 563, 562, 560, 559, 557, 555, 554, 553, 552, 551, 550, 547, 544, 542, 536, 534, 531, 526, 523, 520, 514, 511, 507, 500, 496, 492, 481, 476, 471, 461, 457, 452, 441, 436, 431, 422, 417, 413, 404, 399, 393, 384, 379, 376, 369, 366, 364, 359, 356, 354, 349, 348, 346, 345, 345, 344, 343, 343, 342, 342, 341, 341, 341, 341, 341, 340, 340, 339, 337, 336, 334, 333};
    for (int i = 0; i < 403; i++){
      data_0[i] = y_data_template[i] * sense;}
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);}
    
// Junctional rhythm data
  if (number == 1){
    const short y_data_template[] = {};//{344, 344, 344, 344, 343, 343, 341, 341, 340, 337, 335, 333, 329, 327, 325, 319, 314, 309, 295, 286, 277, 253, 240, 228, 208, 202, 247, 291, 336, 381, 425, 470, 515, 559, 604, 649, 693, 738, 783, 827, 872, 917, 961, 1006, 1051, 1095, 1140, 1185, 1229, 1274, 1219, 1164, 1109, 1054, 999, 944, 889, 834, 779, 724, 669, 614, 559, 504, 449, 394, 339, 284, 229, 174, 119, 64, 9, 25, 40, 56, 72, 87, 103, 119, 134, 150, 165, 181, 197, 212, 228, 244, 259, 275, 291, 299, 306, 316, 320, 323, 325, 326, 327, 328, 330, 331, 333, 334, 334, 334, 335, 335, 336, 336, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 337, 336, 336, 336, 336, 335, 335, 335, 335, 335, 335, 335, 335, 335, 335, 336, 337, 337, 338, 339, 339, 340, 341, 342, 342, 343, 344, 345, 348, 349, 350, 353, 354, 357, 360, 362, 364, 368, 370, 372, 377, 379, 381, 386, 389, 391, 397, 399, 402, 408, 412, 415, 421, 424, 427, 434, 437, 440, 446, 449, 452, 459, 462, 465, 471, 474, 477, 483, 486, 488, 494, 497, 500, 506, 508, 510, 514, 516, 518, 521, 524, 525, 529, 531, 532, 535, 537, 538, 540, 541, 542, 544, 544, 545, 547, 547, 547, 548, 549, 549, 550, 550, 549, 548, 548, 548, 547, 546, 545, 543, 542, 540, 538, 537, 535, 531, 529, 527, 522, 520, 518, 513, 510, 507, 501, 498, 495, 488, 485, 482, 475, 471, 468, 460, 456, 452, 445, 441, 437, 430, 426, 423, 416, 412, 408, 400, 397, 394, 389, 386, 384, 378, 376, 373, 368, 366, 364, 360, 359, 357, 354, 352, 350, 349, 348, 348, 347, 346, 346, 345, 345, 344};
    for (int i = 0; i < 300; i++){
      data_1[i] = y_data_template[i] * sense;}
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);}
  
// First-degree AV block rhythm data
  if (number == 2){
    const short y_data_template[] = {};//{321, 322, 323, 326, 328, 330, 336, 339, 342, 351, 355, 359, 366, 370, 375, 382, 386, 389, 398, 403, 407, 416, 419, 422, 428, 430, 432, 437, 440, 441, 445, 446, 447, 449, 449, 449, 449, 449, 449, 449, 449, 449, 447, 447, 445, 441, 438, 435, 430, 426, 422, 414, 410, 405, 394, 389, 384, 376, 373, 369, 363, 359, 356, 349, 346, 343, 337, 334, 332, 327, 325, 324, 322, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 322, 322, 322, 322, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 322, 322, 322, 321, 321, 321, 320, 319, 317, 315, 309, 306, 303, 294, 290, 285, 254, 239, 222, 200, 195, 191, 241, 291, 341, 391, 441, 491, 541, 591, 641, 691, 741, 792, 842, 892, 942, 992, 1042, 1092, 1142, 1192, 1242, 1292, 1342, 1293, 1244, 1195, 1146, 1096, 1047, 998, 949, 900, 851, 802, 753, 704, 654, 605, 556, 507, 458, 409, 360, 311, 262, 212, 163, 114, 65, 16, 34, 51, 69, 87, 105, 122, 140, 158, 175, 193, 211, 229, 246, 264, 271, 276, 285, 291, 296, 306, 311, 315, 319, 320, 320, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 321, 322, 323, 324, 326, 330, 332, 334, 338, 340, 341, 344, 345, 346, 349, 351, 354, 358, 361, 363, 368, 371, 373, 377, 380, 383, 391, 394, 398, 404, 407, 410, 416, 419, 423, 432, 436, 440, 446, 449, 452, 459, 463, 467, 474, 478, 482, 488, 492, 496, 502, 504, 507, 511, 513, 515, 520, 522, 524, 527, 528, 529, 531, 532, 534, 537, 538, 539, 539, 539, 539, 539, 539, 539, 540, 540, 540, 539, 539, 539, 539, 539, 539, 538, 537, 535, 533, 532, 531, 529, 529, 528, 525, 523, 521, 516, 514, 512, 506, 504, 501, 495, 492, 489, 483, 479, 474, 465, 460, 456, 449, 445, 442, 433, 428, 422, 412, 408, 404, 395, 391, 386, 377, 373, 369, 363, 360, 357, 351, 348, 345, 339, 336, 333, 328, 325, 324, 321, 321};
    for (int i = 0; i < 497; i++){
      data_2[i] = y_data_template[i] * sense;}
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);}
    
// Sinus tachycardia data
  if (number == 3){
    const short y_data_template[] = {315, 316, 316, 316, 317, 317, 317, 318, 318, 319, 320, 321, 322, 324, 325, 326, 327, 328, 330, 332, 333, 336, 338, 340, 343, 345, 348, 350, 352, 355, 358, 360, 363, 366, 369, 371, 374, 377, 379, 382, 385, 388, 390, 393, 395, 397, 399, 402, 404, 406, 407, 409, 410, 411, 411, 412, 413, 413, 413, 414, 414, 414, 414, 414, 414, 414, 414, 413, 413, 413, 412, 410, 409, 407, 405, 403, 401, 397, 394, 391, 388, 385, 381, 378, 374, 370, 366, 363, 359, 355, 352, 348, 345, 342, 339, 336, 333, 330, 327, 324, 322, 320, 318, 316, 315, 313, 311, 309, 308, 307, 307, 307, 307, 307, 307, 307, 307, 307, 307, 306, 305, 304, 304, 303, 303, 302, 302, 301, 299, 297, 295, 291, 287, 283, 278, 274, 269, 263, 258, 250, 243, 233, 225, 215, 204, 192, 180, 167, 156, 152, 156, 179, 208, 250, 300, 347, 399, 451, 514, 580, 651, 734, 818, 897, 989, 1075, 1143, 1209, 1254, 1267, 1249, 1213, 1170, 1100, 1028, 957, 876, 791, 686, 584, 477, 383, 280, 220, 145, 99, 59, 37, 26, 37, 52, 67, 82, 97, 111, 125, 139, 152, 165, 176, 186, 200, 212, 224, 238, 252, 261, 271, 281, 290, 297, 304, 309, 313, 315, 316, 316, 317, 317, 317, 317, 317, 317, 317, 317, 317, 317, 317, 317, 317, 317, 317, 318, 318, 319, 321, 322, 324, 326, 328, 331, 333, 336, 339, 342, 345, 348, 352, 355, 359, 362, 366, 370, 374, 378, 382, 386, 390, 394, 398, 402, 407, 411, 414, 417, 421, 424, 427, 430, 434, 437, 440, 442, 444, 447, 449, 451, 453, 456, 458, 459, 461, 463, 465, 467, 468, 470, 471, 472, 473, 473, 474, 475, 475, 476, 477, 477, 478, 479, 479, 479, 479, 479, 479, 479, 479, 479, 478, 477, 477, 476, 476, 475, 474, 474, 473, 471, 470, 469, 467, 465, 463, 461, 459, 458, 456, 454, 452, 449, 447, 444, 442, 440, 437, 434, 431, 429, 426, 423, 420, 417, 413, 410, 406, 402, 399, 395, 391, 387, 383, 379, 375, 371, 367, 363, 359, 356, 352, 349, 345, 342, 338, 335, 332, 329, 326, 324, 322, 320, 318, 317, 316, 316, 315, 315, 314, 314, 313, 313, 313};
    for (int i = 0; i < 380; i++){
      data_3[i] = y_data_template[i] * sense;}
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);}
    
// Focal atrial tachycardia data
  if (number == 4){
    const short y_data_template[] = {};//{203, 203, 203, 202, 201, 200, 199, 198, 196, 194, 191, 189, 186, 183, 180, 175, 170, 165, 160, 155, 149, 144, 138, 131, 124, 117, 110, 104, 97, 92, 86, 81, 75, 69, 63, 58, 55, 55, 58, 64, 72, 79, 86, 93, 100, 105, 112, 120, 127, 134, 143, 151, 159, 168, 176, 181, 187, 191, 194, 197, 199, 201, 202, 203, 203, 204, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 204, 204, 204, 204, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 205, 204, 204, 204, 203, 201, 199, 196, 193, 189, 186, 183, 179, 176, 173, 169, 168, 212, 256, 300, 344, 388, 433, 477, 521, 565, 609, 653, 697, 741, 785, 829, 873, 917, 962, 1006, 1050, 1094, 1138, 1182, 1139, 1096, 1053, 1010, 968, 925, 882, 839, 796, 753, 710, 667, 624, 582, 539, 496, 453, 410, 367, 324, 281, 238, 196, 153, 110, 67, 24, 27, 35, 49, 60, 73, 85, 97, 107, 119, 129, 138, 148, 158, 165, 176, 186, 194, 199, 203, 204, 204, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 203, 204, 204, 204, 204, 204, 204, 204, 205, 206, 207, 209, 210, 211, 213, 214, 215, 217, 219, 221, 223, 226, 228, 230, 233, 235, 238, 240, 243, 246, 248, 251, 254, 257, 259, 262, 265, 268, 271, 274, 278, 281, 285, 289, 292, 296, 300, 304, 308, 314, 318, 323, 328, 333, 337, 342, 347, 353, 359, 366, 372, 378, 384, 390, 396, 402, 408, 414, 420, 426, 432, 437, 443, 449, 454, 459, 465, 470, 475, 481, 487, 492, 497, 501, 505, 509, 513, 517, 521, 524, 527, 530, 532, 535, 536, 538, 539, 540, 540, 541, 540, 539, 538, 535, 532, 526, 520, 512, 503, 493, 484, 473, 464, 454, 444, 433, 423, 412, 401, 387, 374, 361, 351, 339, 330, 321, 311, 301, 293, 286, 280, 275, 270, 265, 262, 258, 254, 251, 248, 245, 242, 240, 238, 235, 233, 231, 229, 227, 225, 223, 221, 219, 217, 216, 214, 213, 212, 210, 209, 207, 206, 205, 204, 204, 204, 203};
    for (int i = 0; i < 363; i++){data_4[i] = y_data_template[i] * sense;}
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);}
    
// Atrial flutter 4:1 data
  if (number == 5){
    const short y_data_template[] = {223, 233, 243, 251, 258, 264, 269, 276, 282, 289, 299, 309, 318, 327, 335, 341, 345, 350, 355, 359, 361, 363, 362, 360, 356, 352, 348, 343, 337, 332, 326, 321, 317, 313, 309, 304, 299, 293, 287, 281, 275, 269, 263, 257, 251, 245, 239, 232, 227, 221, 216, 210, 205, 200, 194, 189, 185, 181, 178, 174, 171, 168, 165, 162, 160, 158, 156, 155, 153, 151, 150, 149, 147, 146, 145, 143, 142, 141, 139, 138, 136, 134, 131, 128, 126, 123, 118, 113, 107, 100, 94, 90, 86, 83, 81, 79, 78, 77, 76, 75, 74, 73, 73, 72, 72, 71, 72, 73, 76, 79, 84, 89, 94, 100, 107, 115, 123, 132, 142, 152, 161, 171, 181, 191, 201, 211, 220, 229, 238, 247, 257, 267, 278, 289, 299, 309, 318, 327, 334, 342, 348, 355, 362, 368, 375, 381, 387, 393, 398, 403, 407, 411, 413, 414, 415, 413, 412, 409, 406, 402, 398, 394, 389, 383, 377, 371, 365, 359, 353, 347, 340, 333, 325, 318, 310, 302, 294, 286, 278, 269, 260, 245, 229, 214, 199, 184, 175, 171, 171, 224, 278, 331, 384, 438, 491, 544, 597, 651, 704, 757, 811, 864, 917, 971, 1024, 1077, 1130, 1184, 1237, 1186, 1135, 1084, 1032, 981, 930, 879, 828, 777, 725, 674, 623, 572, 521, 470, 418, 367, 316, 265, 214, 163, 111, 60, 9, 10, 12, 19, 26, 32, 38, 43, 42, 40, 37, 34, 30, 27, 23, 20, 16, 12, 8, 5, 4, 2, 2, 2, 4, 6, 10, 14, 18, 23, 28, 33, 39, 46, 53, 60, 67, 74, 82, 90, 99, 109, 121, 132, 144, 155, 168, 180, 190, 201, 212, 222, 233, 245, 258, 272, 286, 298, 308, 319, 327, 336, 343, 350, 355, 357, 357, 355, 352, 348, 343, 336, 329, 322, 314, 306, 298, 291, 284, 278, 271, 265, 259, 252, 246, 240, 233, 227, 221, 215, 209, 203, 198, 192, 187, 182, 177, 172, 168, 165, 161, 159, 156, 154, 152, 149, 147, 145, 143, 140, 138, 136, 134, 131, 128, 124, 120, 115, 110, 104, 99, 95, 91, 88, 87, 85, 83, 82, 81, 79, 78, 77, 76, 76, 76, 77, 78, 80, 82, 85, 88, 93, 97, 103, 109, 116, 124, 131, 137, 143, 149, 153, 158, 163, 167, 171, 175, 178, 181, 185, 188, 191, 194, 197, 200, 204, 208, 212, 217, 221, 226, 230, 235, 239, 244, 248, 251, 255, 259, 262, 266, 270, 275, 280, 287, 294, 301, 308, 314, 320, 325, 328, 332, 335, 337, 339, 339, 338, 336, 334, 330, 326, 322, 318, 313, 308, 304, 299, 295, 292, 287, 283, 278, 272, 265, 258, 251, 245, 241, 236, 231, 225, 219, 213, 207, 201, 197, 193, 189, 186, 182, 179, 176, 173, 170, 167, 164, 162, 159, 157, 154, 152, 150, 148, 146, 144, 142, 140, 137, 135, 132, 128, 124, 119, 113, 107, 101, 96, 91, 89, 87, 85, 83, 82, 80, 79, 78, 77, 76, 75, 74, 73, 73, 72, 71, 70, 70, 70, 70, 70, 71, 72, 73, 75, 77, 80, 85, 90, 96, 101, 107, 114, 122, 131, 141, 151, 162, 172, 182, 191, 200, 209, 217, 225};
    for (int i = 0; i < 536; i++){data_5[i] = y_data_template[i] * sense;}
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);}
    
// Ventricular tachycardia data
  if (number == 6){
    const short y_data_template[] = {1, 3, 8, 14, 30, 46, 62, 76, 93, 100, 108, 117, 128, 136, 150, 162, 180, 195, 211, 222, 239, 250, 264, 277, 296, 309, 323, 335, 350, 360, 372, 384, 400, 412, 425, 435, 448, 456, 465, 474, 488, 500, 516, 531, 545, 554, 568, 578, 593, 609, 624, 633, 666, 693, 728, 762, 804, 823, 846, 862, 884, 898, 915, 928, 942, 952, 973, 990, 1015, 1039, 1069, 1091, 1119, 1139, 1168, 1189, 1212, 1228, 1246, 1256, 1267, 1274, 1283, 1289, 1297, 1304, 1314, 1322, 1331, 1337, 1345, 1351, 1358, 1366, 1374, 1380, 1386, 1392, 1398, 1406, 1412, 1419, 1427, 1433, 1439, 1445, 1451, 1456, 1463, 1469, 1476, 1484, 1492, 1497, 1504, 1509, 1514, 1518, 1523, 1526, 1530, 1534, 1541, 1548, 1555, 1562, 1569, 1573, 1577, 1580, 1582, 1584, 1588, 1592, 1597, 1601, 1605, 1607, 1608, 1609, 1609, 1610, 1610, 1609, 1608, 1607, 1604, 1600, 1597, 1593, 1590, 1587, 1583, 1578, 1573, 1567, 1562, 1558, 1552, 1547, 1541, 1535, 1527, 1519, 1510, 1500, 1486, 1474, 1457, 1444, 1430, 1420, 1409, 1402, 1390, 1378, 1362, 1350, 1337, 1329, 1322, 1319, 1313, 1307, 1293, 1278, 1263, 1247, 1232, 1225, 1220, 1216, 1214, 1212, 1211, 1210, 1217, 1225, 1232, 1241, 1249, 1251, 1261, 1271, 1288, 1305, 1321, 1332, 1342, 1346, 1351, 1355, 1366, 1377, 1392, 1406, 1420, 1426, 1427, 1424, 1393, 1361, 1326, 1294, 1250, 1231, 1207, 1186, 1147, 1119, 1079, 1043, 1000, 975, 926, 890, 833, 784, 711, 663, 590, 539, 477, 438, 390, 365, 342, 329, 293, 266, 225, 183, 141, 123, 89, 70, 52, 34, 17, 17};
    for (int i = 0; i < 260; i++){data_6[i] = y_data_template[i] * sense;}
    NumSample = sizeof(y_data_template) / (2.00 + pembagi / 100);}
}

ISR(TIMER2_OVF_vect)
{
  TCNT2 = tcnt2;
  // Setup data transmission for normal and slow rhythms
if (number == 0) { switch (State) { case INIT: QRSCount = 0; IdleCount = 0; State = QRS; break; case QRS: DTOA_Send(data_0[QRSCount]); QRSCount++; if (QRSCount >= NumSample) { QRSCount = 0; DTOA_Send(data_0[0]); State = IDLE; } break; case IDLE: IdleCount++; if (IdleCount >= IdlePeriod) { IdleCount = 0; State = QRS; } break; default: break; } }

  // Setup data transmission of junctional rhythm
if (number == 1) { switch (State) { case INIT: QRSCount = 0; IdleCount = 0; State = QRS; break; case QRS: DTOA_Send(data_1[QRSCount]); QRSCount++; if (QRSCount >= NumSample) { QRSCount = 0; DTOA_Send(data_1[0]); State = IDLE; } break; case IDLE: IdleCount++; if (IdleCount >= IdlePeriod) { IdleCount = 0; State = QRS; } break; default: break; } }

  // Setup data transmission of first-degree AV block rhythm
if (number == 2) { switch (State) { case INIT: QRSCount = 0; IdleCount = 0; State = QRS; break; case QRS: DTOA_Send(data_2[QRSCount]); QRSCount++; if (QRSCount >= NumSample) { QRSCount = 0; DTOA_Send(data_2[0]); State = IDLE; } break; case IDLE: IdleCount++; if (IdleCount >= IdlePeriod) { IdleCount = 0; State = QRS; } break; default: break; } }

  // Setup data transmission of sinus tachycardia
if (number == 3) { switch (State) { case INIT: QRSCount = 0; IdleCount = 0; State = QRS; break; case QRS: DTOA_Send(data_3[QRSCount]); QRSCount++; if (QRSCount >= NumSample) { QRSCount = 0; DTOA_Send(data_3[0]); State = IDLE; } break; case IDLE: IdleCount++; if (IdleCount >= IdlePeriod) { IdleCount = 0; State = QRS; } break; default: break; } }

  // Setup data transmission of focal artrial tachycardia
if (number == 4) { switch (State) { case INIT: QRSCount = 0; IdleCount = 0; State = QRS; break; case QRS: DTOA_Send(data_4[QRSCount]); QRSCount++; if (QRSCount >= NumSample) { QRSCount = 0; DTOA_Send(data_4[0]); State = IDLE; } break; case IDLE: IdleCount++; if (IdleCount >= IdlePeriod) { IdleCount = 0; State = QRS; } break; default: break; } }

  // Setup data transmission of atrial flutter 4:1
if (number == 5) { TCNT2 = tcnt2; switch (State) { case INIT: QRSCount = 0; IdleCount = 0; State = QRS; break; case QRS: DTOA_Send(data_5[QRSCount]); QRSCount++; if (QRSCount >= NumSample) { QRSCount = 0; DTOA_Send(data_5[0]); State = IDLE; } break; case IDLE: State = QRS; break; default: break; } }

  // Setup data transmission of ventricular tachycardia
if (number == 6) { TCNT2 = tcnt2; switch (State) { case INIT: QRSCount = 0; IdleCount = 0; State = QRS; break; case QRS: DTOA_Send(data_6[QRSCount]); QRSCount++; if (QRSCount >= NumSample) { QRSCount = 0; DTOA_Send(data_6[0]); State = IDLE; } break; case IDLE: State = QRS; break; default: break; } }
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
