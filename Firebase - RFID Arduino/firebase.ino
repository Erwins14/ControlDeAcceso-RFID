#include <ESP8266WiFi.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <SPI.h> 
#include <RFID.h>
#include <FirebaseESP8266.h>  // Install Firebase ESP8266 library
#include <NTPClient.h>
#include <WiFiUdp.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Configura tu red WiFi y los datos de Firebase
#define DATABASE_URL "esp8266-ceb36-default-rtdb.firebaseio.com"
#define API_KEY "AIzaSyAaApo7cviHp2gG-7YnhNCCz9vaUThu1x4"
//#define WIFI_SSID "CLARO_rjNxKd"
//#define WIFI_PASSWORD "C22DCDCC1D"
#define WIFI_SSID "AndroidAP3A73"
#define WIFI_PASSWORD "holahola"

/* Define las credenciales de usuario de Firebase */
#define USER_EMAIL "erwins1414@gmail.com"
#define USER_PASSWORD "UMG2023"

/* Configura el lector RFID */
#define SS_PIN 15  // Pin de selección SPI
#define RST_PIN 16 // Pin de reset

RFID rfid(SS_PIN, RST_PIN);       //D8:pin of tag reader SDA. D0:pin of tag reader RST 
unsigned char str[16]; //MAX_LEN is 16: size of the array 
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display

// Define NTP Client to get time
const long utcOffsetInSeconds = -21600;  // Ajusta el desfase horario según tu ubicación (GMT-6)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;
String nombreUsuario;
String apellidoUsuario;

/* Define la estructura de configuración de Firebase */
FirebaseConfig config;
FirebaseAuth auth;

//String uidPath= "/";
FirebaseJson json;
//Define FirebaseESP8266 data object
FirebaseData firebaseData;
FirebaseData firebaseStatus;

unsigned long lastMillis = 0;
const int red = 2;
const int green = 0;
String alertMsg;
String device_id="device11";
boolean checkIn = true;

void connect() {
  Serial.println("Conectando a WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 30) {
    delay(1000);
    Serial.print(".");
    attempt++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Conectado a WiFi");
  } else {
    Serial.println("Fallo en la conexión a WiFi. Reiniciando...");
    ESP.restart();
  }
}

void setup()
{

  Serial.begin(115200);

  pinMode(red, OUTPUT);
  pinMode(green, OUTPUT);
  lcd.init();                      // initialize the lcd 
  lcd.clear();
  lcd.backlight();

  SPI.begin();
  rfid.init();

  //Conexión WIFI
  connect();
  
  // Initialize a NTPClient to get time
  timeClient.begin();

  /* Configura la estructura de configuración de Firebase */
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  /* Asigna la función de devolución de llamada para la generación de tokens */
  config.token_status_callback = tokenStatusCallback;

  /* Establece el tamaño del búfer BSSL para SSL */
  firebaseData.setBSSLBufferSize(4096, 1024);
  
  Firebase.reconnectWiFi(true);
  /* Inicializa Firebase */
  Firebase.begin(&config, &auth);
  
  //Set database read timeout to 1 minute (max 15 minutes)
  Firebase.setReadTimeout(firebaseData, 1000 * 60);
  Firebase.setReadTimeout(firebaseStatus, 1000 * 60);
  //tiny, small, medium, large and unlimited.
  //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
  Firebase.setwriteSizeLimit(firebaseData, "large");

  Serial.println("ESCANEA RFID");
}

void checkAccess (String temp)    //Function to check if an identified tag is registered to allow access
{
    
    //Check Tarjeta Registrada en Firebase
    if(Firebase.getInt(firebaseData, "/users/"+temp)){

      //Check Usuario Registrado en Firebase
      Firebase.getInt(firebaseStatus, "/usuarios/"+temp+"/userStatus");
      if(firebaseStatus.intData() == 1){
      
          if (firebaseData.intData() == 0)  //Check In
          {  
              alertMsg="CHECK IN";
              lcd.clear();
              lcd.setCursor(3,0);   
              lcd.print("BIENVENIDO");
              lcd.setCursor(4,1);   
              lcd.print(alertMsg);
              delay(2000);

              //Función para Fecha y Hora
              dateTime();

              //Set de Nombre y Apellido de Usuario
              Firebase.getString(firebaseData, "/usuarios/"+temp+"/nombre");
              nombreUsuario = firebaseData.stringData();
              Firebase.getString(firebaseData, "/usuarios/"+temp+"/apellido");
              apellidoUsuario = firebaseData.stringData();
              
              json.add("date", dayStamp);
              json.add("time", timeStamp);
              json.add("id", device_id);
              json.add("uid", temp);
              json.add("nombre", nombreUsuario);
              json.add("apellido", apellidoUsuario);
              json.add("status",1);
    
              Firebase.setInt(firebaseData, "/users/"+temp,1);
              
              if (Firebase.pushJSON(firebaseData, "/asistencia/", json)) {
                Serial.println(firebaseData.dataPath() + firebaseData.pushName()); 
              } else {
                Serial.println(firebaseData.errorReason());
              }
          }
          else if (firebaseData.intData() == 1)   //Check Out 
          { 
              alertMsg="CHECK OUT";
              lcd.clear();
              lcd.setCursor(3,0);   
              lcd.print("HASTA LUEGO");
              lcd.setCursor(4,1);   
              lcd.print(alertMsg);
              delay(2000);
    
              Firebase.setInt(firebaseData, "/users/"+temp,0);
              
              //Función para Fecha y Hora
              dateTime();

              //Set de Nombre y Apellido de Usuario
              Firebase.getString(firebaseData, "/usuarios/"+temp+"/nombre");
              nombreUsuario = firebaseData.stringData();
              Firebase.getString(firebaseData, "/usuarios/"+temp+"/apellido");
              apellidoUsuario = firebaseData.stringData();
              
              json.add("date", dayStamp);
              json.add("time", timeStamp);
              json.add("id", device_id);
              json.add("uid", temp);
              json.add("nombre", nombreUsuario);
              json.add("apellido", apellidoUsuario);
              json.add("status",0);
              if (Firebase.pushJSON(firebaseData, "/asistencia/", json)) {
                Serial.println(firebaseData.dataPath() + firebaseData.pushName()); 
              } else {
                Serial.println(firebaseData.errorReason());
              }
          }
       }
       else if (firebaseStatus.intData() == 0){  //Usuario no registrado
          Serial.println("ACCESO DENEGADO");
          lcd.clear();
          lcd.setCursor(1,0);   
          lcd.print("ACCESO DENEGADO");
          delay(2000);
       }
       
    }
    else
    {
      Serial.println("FALLIDO");
      Serial.println("RAZÓN: " + firebaseData.errorReason());
      pushCard(temp);
    }
       
}

void pushCard (String temp)    //Function to check if an identified tag is registered to allow access
{   
   Serial.println("REGISTRANDO ID TARJETA: "+temp);

    if (Firebase.setInt(firebaseData, "/users/"+temp,0)){

      //Creación de Usuario Nuevo
      Firebase.setString(firebaseData, "/usuarios/"+temp+"/cardUID",temp);
      Firebase.setString(firebaseData, "/usuarios/"+temp+"/nombre"," ");
      Firebase.setString(firebaseData, "/usuarios/"+temp+"/apellido"," ");
      Firebase.setInt(firebaseData, "/usuarios/"+temp+"/userStatus",0);
      Serial.println("Tarjeta registrada en Firebase.");
      lcd.clear();
      lcd.setCursor(1,0);   
      lcd.print("ID REGISTRADO");
      delay(2000);
      
    } else {
      Serial.println("Error al registrar la tarjeta en Firebase: " + firebaseData.errorReason());
      
    }
}

//Función para Fecha y Hora
void dateTime(){
  if(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formattedDate = timeClient.getFormattedDate();

  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
  // Extract time
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);
  delay(300);
  
  //Mostrando Hora y Fecha en LCD
  lcd.clear();
  lcd.setCursor(3,0);   
  lcd.print(dayStamp);
  lcd.setCursor(4,1);   
  lcd.print(timeStamp);
  delay(2000);
}

void loop() {
  
  if (rfid.findCard(PICC_REQIDL, str) == MI_OK)   //Espera a que se coloque una tarjeta cerca del lector
  { 
    Serial.println("Tarjeta Encontrada"); 
    String temp = "";                             //Variable temporal para almacenar el código RFID leído
    if (rfid.anticoll(str) == MI_OK)              //Detección Anticolisión, lee el código de la tarjeta
    { 
      for (int i = 0; i < 4; i++)                 //Registre y muestre el código de la tarjeta
      { 
        temp = temp + (0x0F & (str[i] >> 4)); 
        temp = temp + (0x0F & str[i]); 
      } 
      Serial.println("El ID de la tarjeta es : " + temp); 
      checkAccess(temp);     //Compruebe si la tarjeta identificada esta registrada
    } 
    rfid.selectTag(str); //Bloquea la tarjeta para evitar una lectura redundante
  }
  rfid.halt();

  lcd.clear();
  lcd.setCursor(2,0);   
  lcd.print("ESCANEA RFID");
  lcd.setCursor(5,1);   
  lcd.print("CERRADO");
  delay(500);
}
