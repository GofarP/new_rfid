#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FirebaseESP32.h>
#include "time.h"

#define SS_PIN 21
#define RST_PIN 5
#define FIREBASE_HOST "https://kuncipintu-60301-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_AUTH "N0LjVCJd52Ibxeh7kWyHBJaDfZcfQRc46eOJhbyT"


StaticJsonDocument<700> doc;
HTTPClient http;

String DEVICE_ID="77120552";
String WIFI_SSID = "test";
String WIFI_PASSWORD = "12345679";
String googlDotCom = "www.google.com";
String BASE_URL = "https://fcm.googleapis.com/fcm/send";
String FBStatus;
String id = "";
String username = "";
String statusPintu="tutup";
String deviceTokenKey = ""; //device token key
String response;
String body;

int relay = 5 ; 
int buzzer = 13;
int sensorMagnet = 12;
int merah = 14;
int hijau = 27;
int sensorSentuh = 16; // capactitive touch sensorMagnet - Arduino Digital pin D1
int stateSensorMagnet;
int stateSensorSentuh;
int jumlahInvalid=0;
int randomNumber;

bool ditemukan = false;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 6 * 3600;
const int   daylightOffset_sec = 3600;


FirebaseData firebaseData;


MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class

MFRC522::MIFARE_Key key;

// Init array that will store new NUID
byte nuidPICC[3];

void setup() {
  Serial.begin(9600);
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  pinMode(relay, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(merah, OUTPUT);
  pinMode(hijau, OUTPUT);
  pinMode(sensorMagnet, INPUT_PULLUP);
  pinMode(sensorSentuh, INPUT_PULLUP);
  digitalWrite(relay, HIGH);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  connectWifi();
}

void loop() {

  randomNumber = random(1000000);
  stateSensorMagnet = digitalRead(sensorMagnet);
  stateSensorSentuh = digitalRead(sensorSentuh);
  Serial.println(waktuSekarang());
  Serial.println(stateSensorMagnet);
  Serial.println(stateSensorSentuh);

  if(deviceTokenKey=="")
  {
      if(Firebase.getString(firebaseData, "/fcmtoken/"+DEVICE_ID+"/fcmtoken"))
      {
        deviceTokenKey=firebaseData.stringData();
        Serial.println("Token Key Perangkat Aplikasi:"+ deviceTokenKey);
      }
  }

  if(Firebase.getString(firebaseData, "/bukapintu/"+DEVICE_ID+"/status"))
  {
     if(firebaseData.stringData() == "Terkunci")
     {
        digitalWrite(merah,HIGH);
        Serial.println("Pintu Terkunci Otomatis, Silahkan Buka Dari Aplikasi");
        delay(1000);
        return;
     }
  }

  if (stateSensorMagnet == 0)
  {
    if(statusPintu=="buka")
    {
       buzzerPanjang();      
       statusPintu="tutup";
    }
    
    hijauMati();
    digitalWrite(relay, HIGH);
  }

  if (stateSensorSentuh == HIGH)
  {
    hijauHidup();
    buzzerPendek();

    digitalWrite(relay, LOW);
    delay(3000);
    digitalWrite(relay, HIGH);

    hijauMati();

  }


  if (Firebase.getString(firebaseData, "/bukapintu/"+DEVICE_ID+"/status"))
  {
    if (firebaseData.stringData() == "Buka")
    {

      merahMati();
      hijauHidup();

      Serial.println("Pintu Dibuka...");
      buzzerPendek();
      digitalWrite(relay, LOW);
      jumlahInvalid=0;

      delay(3000);

      if (stateSensorMagnet == 0)
      {
        if (Firebase.setString(firebaseData, "/bukapintu/"+DEVICE_ID+"/status/", "Tutup"))
        {
          hijauMati();
          digitalWrite(relay, HIGH);
          buzzerPanjang();
        }
      }
    }
  }

  Serial.println("Silahkan Scan E-KTP Anda");

  delay(500);
  // Look for new cards
  if ( ! rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  Serial.println(F("A new card has been detected."));

  // Store NUID into nuidPICC array
  for (byte i = 0; i < 4; i++) {
    nuidPICC[i] = rfid.uid.uidByte[i];
  }

  id = String(printHexToDecimal(printHex(rfid.uid.uidByte, rfid.uid.size)));

  Serial.println("Id Pengguna:" + id);

  if (Firebase.getString(firebaseData, "/pengguna/"+DEVICE_ID+"/"+ id))
  {
    Serial.println(id + " Ditemukan");
    statusPintu="buka";
    ditemukan = true;
    buzzerPendek();
    digitalWrite(relay, LOW);
    hijauHidup();

    delay(3000);


  }


  else
  {
    merahHidup();
    Serial.println("ID Tidak Ditemukan");
    id = "";
    buzzerPanjang();
    merahMati();
    jumlahInvalid++;

    if (Firebase.setString(firebaseData, "/notifikasi/"+ DEVICE_ID +"/" +String(randomNumber) + "/notifikasi", "Seseorang tidak dikenal telah mengakses pintu"))
    {
      Serial.println("Sukses Mengirim Notifikasi Pengguna Tidak Dikenal");
    }

    delay(2000);

    if (Firebase.setString(firebaseData, "/notifikasi/"+ DEVICE_ID + "/" + String(randomNumber) + "/time", waktuSekarang()))
    {
      Serial.println("Sukses Mengirim Time");
    }

    if(jumlahInvalid==10)
    {
      Firebase.setString(firebaseData, "/bukapintu/"+DEVICE_ID+"/status/", "Terkunci");
      sendPopUp("Pintu Dikunci ", "Silahkan Buka Aplikasi Android");
      delay(2000);

      if (Firebase.setString(firebaseData, "/notifikasi/"+ DEVICE_ID +"/" +String(randomNumber) + "/notifikasi", "Pintu Terkunci Otomatis, Silahkan Buka Dengan Aplikasi "))
      {
        Serial.println("Sukses Mengirim Notifikasi Pintu Terkunci");
      }

      delay(2000);

      if (Firebase.setString(firebaseData, "/notifikasi/"+ DEVICE_ID + "/" + String(randomNumber) + "/time", waktuSekarang()))
      {
        Serial.println("Sukses Mengirim Time");
      }

    
      return;
    }


    sendPopUp("Pengguna Tidak Dikenal", "Hati-Hati, Ada Pengguna Tidak Dikenal Mencoba Mengakses Pintu");

  }

  if (ditemukan)
  {
    if (Firebase.getString(firebaseData, "/pengguna/" + DEVICE_ID+"/"+ id + "/username"))
    {
      username = firebaseData.stringData();
    }


    if (Firebase.setString(firebaseData, "/notifikasi/"+ DEVICE_ID +"/" +String(randomNumber) + "/notifikasi", username + " telah mengakses pintu dengan menggunakan E-KTP"))
    {
      Serial.println("Sukses Mengirim Username");
    }

    delay(2000);

    if (Firebase.setString(firebaseData, "/notifikasi/"+ DEVICE_ID + "/" + String(randomNumber) + "/time", waktuSekarang()))
    {
      Serial.println("Sukses Mengirim Time");
    }


    sendPopUp("Pintu Dibuka",username+" Telah Membuka Pintu Dengan Menggunakan KTP");

    ditemukan = false;
    username = "";
    id = "";
    jumlahInvalid=0;
  }



  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();

}


/**
   Helper routine to dump a byte array as hex values to Serial.
*/
String printHex(byte *buffer, byte bufferSize) {
  String hasilDex = "";

  for (byte i = 0; i < bufferSize; i++) {

    if (hasilDex.length() < 7)
    {
      if (buffer[i] < 0x10)
      {
        hasilDex += "0" + String(buffer[i], HEX);
      }

      else
      {
        hasilDex += String(buffer[i], HEX);
      }
    }

  }

  return hasilDex;

}

int printHexToDecimal(String value)
{

  //hasil=1781106690
  Serial.println("value:" + value);
  int val = 0;
  int pangkat = 0;
  for (int i = 0; i < value.length(); i++)
  {
    pangkat = pow(16, value.length() - i - 1);

    if (value[i] == 'a')
    {
      val += 10 * pangkat;
    }

    else if (value[i] == 'b')
    {
      val += 11 * pangkat;
    }

    else if (value[i] == 'c')
    {
      val += 12 * pangkat;
    }

    else if (value[i] == 'd')
    {
      val += 13 * pangkat;
    }

    else if (value[i] == 'e')
    {
      val += 14 * pangkat;
    }

    else if (value[i] == 'f')
    {
      val += 15 * pangkat;
    }

    else
    {
      val += String(value[i]).toInt() * pangkat;
    }
  }

  return val;
}

/**
   Helper routine to dump a byte array as dec values to Serial.
*/
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : "");
    Serial.print(buffer[i], DEC);
  }
}

void printRandom(String value)
{
  //  String value="6A298802";

  int val = 0;
  int pangkat = 0;
  for (int i = 0; i < value.length(); i++)
  {
    pangkat = pow(16, value.length() - i - 1);

    if (value[i] == 'A')
    {
      val += 10 * pangkat;
    }

    else if (value[i] == 'B')
    {
      val += 11 * pangkat;
    }

    else if (value[i] == 'C')
    {
      val += 12 * pangkat;
    }

    else if (value[i] == 'D')
    {
      val += 13 * pangkat;
    }

    else if (value[i] == 'E')
    {
      val += 14 * pangkat;
    }

    else if (value[i] == 'F')
    {
      val += 15 * pangkat;
    }

    else
    {
      val += String(value[i]).toInt() * pangkat;
    }
    pangkat = 0;
  }

  Serial.println(val);
}

void connectWifi()
{
  //menampilkan menyambungkan ke wifi
  Serial.println("Connecting To Wifi");
  //mulai menghubungkan dengan ssid(nama wifi) dan password wifi diatas
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());

  //selama belum terhubung tampilkan titik titik
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println("Wifi Connected");
  Serial.println(WiFi.SSID());
  Serial.println(WiFi.macAddress());

  //lakukan test koneksi ke google
  if (Ping.ping(googlDotCom.c_str()))
  {
    Serial.println("Connected to Internet");
  }

}

void merahHidup()
{
  digitalWrite(merah, HIGH);
}

void merahMati()
{
  digitalWrite(merah, LOW);
}

void hijauHidup()
{
  digitalWrite(hijau, HIGH);
}

void hijauMati()
{
  digitalWrite(hijau, LOW);
}


void buzzerPendek()
{

  for (int i = 0; i <= 1; i++)
  {
    digitalWrite(buzzer, HIGH);
    delay(200);
    digitalWrite(buzzer, LOW);
    delay(200);
  }

}


void buzzerPanjang()
{
  digitalWrite(buzzer, HIGH);
  delay(1000);
  digitalWrite(buzzer, LOW);
}


String waktuSekarang()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }


  int tanggal = timeinfo.tm_mday;
  int bulan = timeinfo.tm_mon;
  int tahun = timeinfo.tm_year + 1900;

  int jam = timeinfo.tm_hour;
  int menit = timeinfo.tm_min;
  int detik = timeinfo.tm_sec;

  String waktu = String(tanggal) + "-" + String(bulan) + "-" + String(tahun) + " " + String(jam) + ":" + String(menit) + ":" + String(detik);

  return waktu;

}


void sendPopUp(String judul, String isi)
{
  body="";
  http.begin(BASE_URL);
  http.addHeader("Authorization", "key=AAAAviyvsLI:APA91bGGnqVeKaOcjViI6Xpj_K21Rj4_2o_V-5rWH5t6rLsk32IwybUIJ_vbzpBX08VaOCnhf9g5CL3dwAwVuGq1xRFEeDmUThOkkVb_9LxK0WU9wSBMIRqPJfDz8KkAdCXROG8znZ6V");
  http.addHeader("Content-Type", "application/json");

  doc["to"] = deviceTokenKey;

  JsonObject notificationObject = doc.createNestedObject("notification");
  notificationObject["body"] = isi;
  notificationObject["OrganizationId"] = 2;
  notificationObject["content_available"] = true;
  notificationObject["priority"] = "high";
  notificationObject["subtitle"] = "Notifikasi ESP-32";
  notificationObject["title"] = judul;

  serializeJson(doc, body);
  Serial.println(body);

  http.POST(body);
  response = http.getString();
  Serial.println("response:"+response);


}
