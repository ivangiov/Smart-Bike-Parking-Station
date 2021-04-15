
//  Deskripsi Dasar --------------------------------------------------------------------------

//
//  Software Teorema Rantai
//
//  --deskripsi--
//
//  Disusun oleh:
//  1. 16518017 | Gregorius Jovan Kresnadi
//  2. 16518077 | Muhammad Rizky Ismail Faizal
//  3. 16518137 | Salma Majidah
//  4. 16518197 | Ivan Giovanni
//  5. 16518359 | Valentinus Devin Setiadi
//

//  Kebutuhan Awal ---------------------------------------------------------------------------

#include <Key.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>

//  Pemetaan PIN -----------------------------------------------------------------------------

//    Digital
// D0   | 0   - 
// D1   | 1   - MQTT
// D2   | 2   - Keypad 7 
// D3   | 3   - Keypad 6 
// D4   | 4   - Keypad 5
// D5   | 5   - Keypad 4
// D6   | 6   - Keypad 3
// D7   | 7   - Keypad 2
// D8   | 8   - Keypad 1
// D9   | 9   - RFID Reset Pin
// D10  | 10  - RFID SS Pin 
// D11  | 11  - RFID SPI MOSI
// D12  | 12  - RFID SPI MISO
// D13  | 13  - RFID SPI SCK

//    Analog (digunakan untuk output digital)
// A0   | 14  Relay ke Solenoid 1 (besar) 
// A1   | 15  Relay ke Solenoid 2 (kecil)
// A2   | 16  Buzzer

//  Persiapan Lokal --------------------------------------------------------------------------

// RFID
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);

// Buzzer
#define BUZZER 16

// LCD
LiquidCrystal_I2C lcd(0x27,16,2);

// Keypad
const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {2, 3, 4, 5}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {6, 7, 8}; //connect to the column pinouts of the keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

// Data Lainnya
const String greeting = "Selamat datang! Tempelkan kartu!";
const int delayTime = 10000;   // Waktu solenoid
const int textTime = 2000;    // Waktu tulisan kalo ada yang gagal
const int scanTime = 500;     // Waktu nunggu sebelum scan RFID lagi
const int keypadTime = 10000; // Waktu nunggu masukan di keypad
const int BIKE_COUNT = 2;
String bikes[BIKE_COUNT] = {"0", "0"};
const int MAX_MEMBER_COUNT = 15;
String members[MAX_MEMBER_COUNT] = {"C6 51 71 F5", "36 BB B7 C3", "8F 56 81 A3", "", "", "", "", "", "", "", "", "", "", "", ""};
const int solenoids[BIKE_COUNT] = {14, 15};
const String version = "2.3";

//  Fungsi Setup -----------------------------------------------------------------------------

void setup()
{
  // LCD Initialization
  lcd.init();
  lcd.backlight();

  // Serial Begin
  Serial.begin(115200);
  SPI.begin();

  // RFC Initialization
  mfrc522.PCD_Init();

  // Buzzer Initialization
  pinMode(BUZZER, OUTPUT);

  // Solenoid Initialization
  for(int i = 0; i < BIKE_COUNT; i++) 
  {
    pinMode(solenoids[i], OUTPUT);
    digitalWrite(solenoids[i], LOW);
  }

  // Greeting
  lcdPrint(greeting);
  Serial.println("Teorema Rantai v" + version + " dimulai...");
  Serial.println();

}

//  Fungsi Loop ------------------------------------------------------------------------------

void loop() 
{

  //  Pencarian Kartu ------------------------------------------------
  
  // Cari kartu baru dan scan.
  if (receiveMessages()) {
    delay(scanTime);
    return;
  }
  
  String content = scanRFID();
  if (content == "") 
  {
    delay(scanTime);
    return;
  }
  else 
  {
    content.toUpperCase();
    sendMQTT("SCANNED|"+content+"|");
    Serial.print("UID tag :");
    Serial.println(content);
    Serial.print("Result : ");
  }

  //  Pengecekan Kartu -----------------------------------------------

  // Pembersihan pesan
  String code = content.substring(1);

  // Jika sudah terdaftar...
  if (isIn(members, MAX_MEMBER_COUNT, code) > -1)
  {
    // ...buzz...
    buzz(200);
    // ...lalu cek apa bikenya ada atau tidak.
    int slot = isIn(bikes, BIKE_COUNT, code);

    // Jika mengambil sepeda...
    if (slot > -1) 
    {
      // ...cek slotnya...
      lcdPrint("Silakan ambil", "pada slot " + String(slot + 1) + "!");
      sendMQTT("Bike of " + code + " released from slot " + String(slot) + ".");
      bikes[slot] = "0";
      buzz(200);
      unlock(slot);
    }
    // Jika menaruh sepeda ...
    else
    {
      // ...pilih pin...
      if (availableSlots() == "Pilihan: ") 
      {
        lcdPrint("Mohon maaf,", "stasiun penuh!");
        buzz(50, 100, 2);
        delay(textTime);
        lcdPrint(greeting);
        return;
      }
      lcdPrint("Masukkan slot!", availableSlots());
      char slotKey = scanKeypad(keypadTime);
      if (slotKey == NO_KEY) 
      {
        lcdPrint(greeting);
        return;
      }

      int slot = slotKey - 49;
      
      // ...lalu cek slot pilihan.
      if (slot > -1 && slot < BIKE_COUNT && slotAvailable(slot)) 
      {
        lcdPrint("Silakan parkir", "pada slot " + String(slot + 1) + "!");
        sendMQTT("Bike of " + code + " placed at slot " + String(slot) + ".");
        bikes[slot] = code;
        buzz(200);
        unlock(slot);
      }
      else 
      {
        lcdPrint("Slot tersebut", "tidak tersedia!");
        sendMQTT("Invalid slot: " + String(slot));
        buzz(50, 100, 4);
        delay(textTime);
      }
    }
    // Update semua data ke cloudMQTT.
    updateMQTT();
  }
  // Jika belum terdaftar...
  else 
  {
    lcdPrint("Anda belum", "terdaftar!");
    sendMQTT("Access denied");
    buzz(50, 100, 5);
    delay(textTime);
  }

  //  Penutupan Loop -------------------------------------------------

  // Di akhir loop, kembalikan ke greeting.
  lcdPrint(greeting);
}

//  Fungsi Lainnya ---------------------------------------------------------------------------

// isIn(arr, len, code)
// Mengecek apakah [code] ada didalam array [arr] atau tidak, dengan [len] sebagai panjang array [arr].
// Jika ada dalam [arr], dikembalikan index [code] dalam [arr]. Jika tidak ada, dikembalikan -1.
int isIn(String arr[], int len, String code) {
  int result = -1;
  for(int i = 0; i < len; i++) {
    if(arr[i] == code){
      result = i;
      break;
    }
  }
  return result;
}

// slotAvailable(slot)
// Mengecek slot [slot]. Jika terisi akan mengembalikan false, jika tidak mengembalikan true.
bool slotAvailable(byte slot) 
{
  return bikes[slot] == "0";
}

// availableSlots()
// Mengembalikan string berisi slot slot yang kosong.
String availableSlots() 
{
  String result = "";
  for(int i = 0; i < BIKE_COUNT; i++) 
  {
    if (slotAvailable(i)) 
    {
      result.concat(", " + String(i+1));
    }
  }
  return "Pilihan: " + result.substring(2);

}

// scanRFID()
// Mencari kartu RFID terdekat setiap 500ms hingga ditemukan kemudian mengembalikan kodenya
String scanRFID() 
{
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) 
  {
    return "";
  }
  String content = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) 
  {
     content.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     content.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  return content;
}

// scanKeypad(timeout)
// Menunggu masukan pada keypad selama [timeout]ms.
char scanKeypad(int timeout) 
{
  if (timeout != 0) 
  {
    unsigned long time = millis();
    while(millis() - time < timeout) 
    {
      char key = keypad.getKey();
      if (key != NO_KEY) 
      {
        return key;
      }
    }
    return NO_KEY;
  }
  else 
  {
    return keypad.waitForKey();
  }
}

// lcdPrint(line1, line2)
// Mencetak baris [line1] dan [line2] pada baris kesatu dan kedua LCD.
void lcdPrint(String line1, String line2) 
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);
}

// lcdPrint(line)
// Mencetak baris [line] pada LCD. Jika terlalu panjang dilanjutkan di baris selanjutnya.
void lcdPrint(String line) 
{
  if (line.length() >= 16) 
  {
    lcdPrint(line.substring(0, 16), line.substring(16));
  }
  else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line);
  }
}

// sendMQTT(message)
// Mengirim pesan dalam [message] ke cloudMQTT.
void sendMQTT(String message) {
  Serial.println(message);
}

// sendMQTT(slot, code)
// Mengirim data slot ke MQTT.
void sendMQTT(String slot, String code) {
  sendMQTT("SET|" + slot + "|" + code + "|");
}

// updateMQTT()
// Mengupload kode RFID untuk tiap sepeda yang diparkirkan ke cloudMQTT. Diupload 0 untuk slot yang kosong.
void updateMQTT() {
  for(int i = 0; i < BIKE_COUNT; i++) 
  {
    sendMQTT(String(i), bikes[i]);
  }
}

// buzz(beep, hold)
// Menyalakan buzzer selama [beep]ms lalu menunggu selama [hold]ms.
void buzz(int beep, int hold) {
  digitalWrite(BUZZER, HIGH);
  if (beep > 0) {
    delay(beep);
  }
  digitalWrite(BUZZER, LOW);
  if (hold > 0) {
    delay(hold);
  }
}

// buzz(beep)
// Menyalakan buzzer selama [beep]ms.
void buzz(int beep) {
  buzz(beep, 0);
}

// buzz(beep, hold, count)
// Menyalakan buzzer selama [beep]ms sebanyak [count] kali dengan delay antara bunyi sebanyak [hold]ms.
void buzz(int beep, int hold, int count) {
  for(int i = 0; i < count; i++) {
    buzz(beep, hold);
  }
}

// unlock(slot)
// Membuka kunci pada slot yang ditentukan. Slot pertama dianggap slot 0.
bool unlock(byte slot) {
  if (slot > -1 && slot < BIKE_COUNT) 
  {
    digitalWrite(solenoids[slot], HIGH);
    delay(delayTime);
    digitalWrite(solenoids[slot], LOW);
    return true;
  }
  else 
  {
    sendMQTT("Tried to unlock solenoid " + String(slot) + ". It doesn't exist.");
    return false;
  }
}

// receiveMessages()
// Membaca pesan dari nodemcu
bool receiveMessages() {
  if (Serial.available() <= 0) {
    return false;
  }
  while (Serial.available() > 0) {
    Serial.println("READING");
    String command = Serial.readStringUntil("|");
    command = command.substring(0, command.length()-1);
    sendMQTT(command);
    if (command == "SET")
    {
      String temp = Serial.readStringUntil("|");
      int slot = temp.substring(0,temp.length()-1).toInt();
      String code = Serial.readStringUntil("|");
      code = code.substring(0, code.length()-1);
      if (code == "0") {
        lcdPrint("Silakan ambil", "pada slot " + String(slot + 1) + "!");
        sendMQTT("Bike of " + code + " released from slot " + String(slot) + ".");
        buzz(200);
        unlock(slot);
        lcdPrint(greeting);
      }
      bikes[slot] = code;
      sendMQTT(slot, code);
    }
    else if (command == "ADD") 
    {
      String code = Serial.readStringUntil("|");
      code = code.substring(0, code.length()-1);
      if (isIn(members, MAX_MEMBER_COUNT, code) == -1) {
        for (int i = 0; i < MAX_MEMBER_COUNT; i++) {
          if (members[i] == "") {
            members[i] = code;
            break;
          }
        }
      }
    }
    else if (command == "DELETE") 
    {
      String code = Serial.readStringUntil("|");
      code = code.substring(0, code.length()-1);
      if (isIn(members, MAX_MEMBER_COUNT, code) > -1) {
        for (int i = 0; i < MAX_MEMBER_COUNT; i++) {
          if (members[i] == code) {
            members[i] = "";
            break;
          }
        }
      }
    }
    else if (command == "PING") 
    {
      lcdPrint("Pesan diterima!", "\"Halo!\" -Android");
      delay(textTime);
      lcdPrint(greeting);
    }
    else if (command == "REFRESH") 
    {
      sendMQTT("REPLY|");
      updateMQTT();
    }
  }
  return true;
}
