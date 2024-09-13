#include <WiFi.h>
#include <WiFIManager.h>
#include <HTTPClient.h>
#include <RF24.h>

//DATABASE----------------------------------------------------------------
const char* supabase_url = "https://wxtqtzxrutaxylozbclg.supabase.co/rest/v1/Datos";
const char* supabase_apikey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Ind4dHF0enhydXRheHlsb3piY2xnIiwicm9sZSI6ImFub24iLCJpYXQiOjE2OTYwOTE1MDQsImV4cCI6MjAxMTY2NzUwNH0.rJ8A4NhsV64tm9EAHD_4rIg-KoDG5jy7-yyjiEpsDIA";
String url =  (String)supabase_url + "?apikey=" + supabase_apikey;
//------------------------------------------------------------------------

//Parámetros NRF----------------------------------------------------------
#define CE_PIN 4
#define CSN_PIN 5
#define SCK_PIN 18
#define MISO_PIN 19
const uint64_t addresses[] = {0x7878787878LL, 0xB3B4B5B6F1LL, 0xB3B4B5B6CDLL, 0xB3B4B5B6A3LL, 0xB3B4B5B60FLL, 0xB3B4B5B605LL };
RF24 radio(CE_PIN ,CSN_PIN);
//------------------------------------------------------------------------

struct Data
{
  int id_modulo;
  float temperatura;
  float humedad;
};
Data data;

uint8_t tries = 0;
bool formato = false;
const String separator = "-----------------------------------------------------------------------------";

//Funciones------------------------------------------------
void start_radio();
String to_JSON(int id_modulo, float temperatura, float humedad);
bool database_POST(String to_JSON);
bool check_data(Data data);
//---------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(LED_BUILTIN,OUTPUT);
  REG_WRITE(GPIO_OUT_W1TS_REG, 0<<LED_BUILTIN ); //Setea LED LOW

  start_radio();
  bool connection = radio.isChipConnected();
  while(!connection){
    delay(2000);
    Serial.println("Conexión: " + (String)connection);
    connection = radio.isChipConnected();
  }
  Serial.println("Conexión NRF24L01 OK");
  
  // Connect to Wi-Fi
  WiFiManager wm;
  if(!wm.autoConnect("ESP_WIFI_CONFIG")){
    Serial.println("Error: Intento de conectar a la Red");
    delay(1000);
    ESP.restart();
  }

  Serial.println(separator);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) { //WiFi Conectado
    REG_WRITE(GPIO_OUT_W1TS_REG, 1<<LED_BUILTIN ); //Setea LED HIGH
    tries = 0;
    if(radio.available()){
      radio.read(&data, sizeof(data));
      formato = check_data(data);//Buscar Valores NaN o absurdos
      Serial.println(separator);
      Serial.println((formato?"(Correcto) ":"(Incorrecto) ") + ("Recibido del módulo " + (String)data.id_modulo + ":  " + (String)data.temperatura + "*C   " + (String)data.humedad + "%HR"));
      if(formato){ 
        if( !database_POST(to_JSON(data.id_modulo,data.temperatura,data.humedad)) ){
          Serial.println("TO.DO guardar en EEPROM");    
        }
      }
    } 
  } else { //WiFi Desconectado
    REG_WRITE(GPIO_OUT_W1TS_REG, 0<<LED_BUILTIN ); //Setea LED LOW
    Serial.println("WiFi Desconectado: Intento " + tries );
    WiFi.disconnect();
    WiFi.reconnect();
    delay(5000);
    if(tries>3){ESP.restart();}else{tries++;}
  }
}

String to_JSON(int id_modulo, float temperatura, float humedad){
  String data = "{\"temperatura\":\""+ (String)temperatura +"\",\"humedad\":\""+ (String)humedad +"\",\"id_modulo\":\""+ (int)id_modulo +"\"}";
  return data;
}
bool check_data(Data data){ //buscar valores NaN y o obsurdos
  bool nan = (data.humedad==data.humedad || data.temperatura==data.temperatura || data.id_modulo==data.id_modulo);
  bool zeros = (data.humedad !=0 || data.temperatura != 0);
  bool extreme = (data.humedad >= 0 && data.humedad <=100) && (data.temperatura >= -5 &&data.temperatura <= 70);
  bool id = (data.id_modulo >= 0 && data.id_modulo <=10);
  return ((nan && zeros && extreme && id)? true:false );
}

bool database_POST(String JSON){
  HTTPClient client;
  client.begin(url);
  client.addHeader("Content-Type", "application/json");
  int http_code = client.POST(JSON);
  if(http_code > 0 ){
    if(http_code == HTTP_CODE_CREATED){
      Serial.println("Datos enviados a Supabase");
      client.end();
      return true;
    } else{
      client.end();
      Serial.println("error: HTTP_CODE: " + (String)http_code);
      return false;
    }
  } else { 
    Serial.println("error: HTTP_CODE: " + (String)http_code);
    client.end();
    return false;
  }
}

void start_radio(){
  radio.begin();
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(100);//2499MHz
  Serial.println(separator);
  Serial.println("NRF24L01 escuchando a los Address: ");
  for(int i = 0 ; i<6 ; i++){
    radio.openReadingPipe(i,addresses[i]);
    Serial.println(" (" + (String)addresses[i] + ") " + " ---> Modulo: " + i);
  }
  Serial.println(separator);
  radio.startListening();
}