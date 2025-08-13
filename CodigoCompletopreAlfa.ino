#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Fingerprint.h>
#include <SD.h>
#include <HardwareSerial.h>
#include <ArduinoJson.h>
#include <RTClib.h>

// Configuración LCD 20x4
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Sensor de huellas (UART2: RX=16, TX=17)
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// RTC DS3231
RTC_DS3231 rtc;

// Pines
#define SD_CS 5
#define BUZZER_PIN 32
#define DOWN_PIN 25
#define UP_PIN 26
#define SELECT_PIN 27

// Variables globales
uint8_t idSeleccionado = 1;
// Variables globales modificadas
uint8_t opcionSeleccionada = 0;
// Configuración mejorada para el menú
const char* opcionesMenu[] = {
  "1.Modo Registro    ",  // 16 caracteres (para LCD 20x4)
  "2.Modo Asistencia  ",
  "3.Eliminar Usuario ",  // Se corta intencionalmente
  "4.Ver Archivos     "
};
uint8_t ultimaOpcion = 255;  // Para control de refresco

// Constantes para el sensor de huellas
#define FINGERPRINT_NOIMAGE 0
#define FINGERPRINT_ERROR 255

void beep(bool success) {
  tone(BUZZER_PIN, success ? 1000 : 300, 200);
  delay(250);
  noTone(BUZZER_PIN);
}

// Función mostrarMenu optimizada
void mostrarMenu() {
  if(opcionSeleccionada != ultimaOpcion) {
    lcd.clear();
    
    for(int i = 0; i < 4; i++) {
      lcd.setCursor(0, i);
      
      if(i == opcionSeleccionada) {
        lcd.print(">");
        lcd.print(opcionesMenu[i]);  // Ahora funciona con String
      } else {
        lcd.print(" ");
        lcd.print(opcionesMenu[i]);
      }
    }
    ultimaOpcion = opcionSeleccionada;
  }
}


String obtenerFechaHoraActual() {
  DateTime ahora = rtc.now();
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
           ahora.year(), ahora.month(), ahora.day(),
           ahora.hour(), ahora.minute(), ahora.second());
  return String(buffer);
}

uint8_t encontrarIDLibre() {
  for(uint8_t id = 1; id <= 127; id++) {
    if(finger.loadModel(id) != FINGERPRINT_OK) {
      return id;
    }
  }
  return 0;
}

uint8_t encontrarSiguienteIDLibre(uint8_t actual) {
  for(uint8_t id = actual + 1; id <= 127; id++) {
    if(finger.loadModel(id) != FINGERPRINT_OK) {
      return id;
    }
  }
  return 0;
}

String obtenerNombrePorID(uint8_t id) {
  File archivo = SD.open("/usuarios.json");
  if(!archivo) return "Desconocido";

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, archivo);
  archivo.close();

  if(error) return "Desconocido";

  JsonObject usuario = doc["ID_" + String(id)];
  return usuario["nombre"] | "Desconocido";
}

String obtenerCursoPorID(uint8_t id) {
  File archivo = SD.open("/usuarios.json");
  if(!archivo) return "Desconocido";

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, archivo);
  archivo.close();

  if(error) return "Desconocido";

  JsonObject usuario = doc["ID_" + String(id)];
  return usuario["curso"] | "Desconocido";
}

void guardarEnJSON(uint8_t id, const String& nombre, const String& curso) {
  String fechaHora = obtenerFechaHoraActual();

  StaticJsonDocument<4096> doc;
  File archivo = SD.open("/usuarios.json", FILE_READ);

  if(archivo) {
    DeserializationError error = deserializeJson(doc, archivo);
    archivo.close();
  }

  JsonObject usuario = doc.createNestedObject("ID_" + String(id));
  usuario["nombre"] = nombre;
  usuario["curso"] = curso;
  usuario["fechaRegistro"] = fechaHora;

  archivo = SD.open("/usuarios.json", FILE_WRITE);
  if(archivo) {
    serializeJsonPretty(doc, archivo);
    archivo.close();
  }
}

void eliminarDelJSON(uint8_t id) {
  File archivo = SD.open("/usuarios.json", FILE_READ);
  StaticJsonDocument<4096> doc;

  if(archivo) {
    DeserializationError error = deserializeJson(doc, archivo);
    archivo.close();
    if(error) return;
  }

  doc.remove("ID_" + String(id));

  archivo = SD.open("/usuarios.json", FILE_WRITE);
  if(archivo) {
    serializeJsonPretty(doc, archivo);
    archivo.close();
  }
}

String solicitarDatoSerial(String mensaje) {
  Serial.println(mensaje);
  String dato = "";
  while(dato == "") {
    if(Serial.available()) {
      dato = Serial.readStringUntil('\n');
      dato.trim();
    }
    delay(100);
  }
  return dato;
}

void errorRegistro(const char* mensaje) {
  lcd.clear();
  lcd.setCursor(2, 1);
  lcd.print("ERROR:");
  lcd.setCursor(2, 2);
  lcd.print(mensaje);
  beep(false);
  delay(1500);
  lcd.clear();
}

void modoRegistro() {
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("REGISTRO RAPIDO");
  delay(500);

  idSeleccionado = encontrarIDLibre();
  if(idSeleccionado == 0) {
    lcd.clear();
    lcd.setCursor(2, 1);
    lcd.print("MEMORIA LLENA!");
    beep(false);
    delay(2000);
    return;
  }

  while(true) {
    lcd.setCursor(0, 1);
    lcd.print("ID Asignado: ");
    lcd.print(idSeleccionado);
    lcd.print("   ");
    lcd.setCursor(0, 2);
    lcd.print("Coloca el dedo...");
    lcd.setCursor(0, 3);
    lcd.print("UP+DOWN 3s=Salir");

    if(finger.getImage() == FINGERPRINT_OK) {
      // [Mantener todo el proceso de registro de huella igual...]
      
      // Después de registrar la huella exitosamente:
      lcd.clear();
      lcd.setCursor(2, 0);
      lcd.print("Registro exitoso!");
      lcd.setCursor(2, 1);
      lcd.print("ID: ");
      lcd.print(idSeleccionado);
      delay(1500);

      // Solicitar nombre por serial
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ingrese nombre por");
      lcd.setCursor(0, 1);
      lcd.print("monitor serial");
      lcd.setCursor(0, 2);
      lcd.print("y presione ENTER");
      
      String nombre = "";
      while(nombre == "") {
        if(Serial.available()) {
          nombre = Serial.readStringUntil('\n');
          nombre.trim();
        }
        delay(100);
      }

      // Mostrar nombre recibido
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Nombre recibido:");
      lcd.setCursor(0, 1);
      lcd.print(nombre.substring(0, 20)); // Ajustar a pantalla
      delay(1500);

      // Solicitar curso por serial
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ingrese curso por");
      lcd.setCursor(0, 1);
      lcd.print("monitor serial");
      lcd.setCursor(0, 2);
      lcd.print("y presione ENTER");
      
      String curso = "";
      while(curso == "") {
        if(Serial.available()) {
          curso = Serial.readStringUntil('\n');
          curso.trim();
        }
        delay(100);
      }

      // Mostrar curso recibido
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Curso recibido:");
      lcd.setCursor(0, 1);
      lcd.print(curso.substring(0, 20)); // Ajustar a pantalla

      // Confirmar datos
      lcd.setCursor(0, 2);
      lcd.print("ID:");
      lcd.print(idSeleccionado);
      lcd.print(" ");
      lcd.print(nombre.substring(0, 12));
      lcd.setCursor(0, 3);
      lcd.print("Curso:");
      lcd.print(curso.substring(0, 14));
      
      beep(true);
      delay(2000);

      // Guardar en JSON
      guardarEnJSON(idSeleccionado, nombre, curso);

      // Auto-incrementar al siguiente ID libre
      idSeleccionado = encontrarSiguienteIDLibre(idSeleccionado);
      if(idSeleccionado == 0) {
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("MEMORIA LLENA!");
        delay(2000);
        return;
      }

      lcd.clear();
    }

    // [Resto del código para salida con UP+DOWN...]
  }
}
void mostrarInfoUsuario(uint8_t id) {
  File archivo = SD.open("/usuarios.json");
  if(!archivo) {
    lcd.setCursor(0, 1);
    lcd.print("Error archivo JSON");
    return;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, archivo);
  archivo.close();

  if(error) {
    lcd.setCursor(0, 1);
    lcd.print("Error JSON");
    return;
  }

  JsonObject usuario = doc["ID_" + String(id)];
  String nombre = usuario["nombre"] | "No registrado";
  String curso = usuario["curso"] | "No registrado";
  String fecha = usuario["fechaRegistro"] | "Sin fecha";

  lcd.setCursor(0, 0);
  lcd.print("ID:");
  lcd.print(id);
  lcd.setCursor(8, 0);
  lcd.print(curso);
  lcd.setCursor(0, 1);
  lcd.print(nombre);
  lcd.setCursor(0, 2);
  lcd.print("Reg: ");
  lcd.print(fecha);
}

void modoEliminarUsuario() {
  uint8_t currentID = 1;
  bool modoConfirmacion = false;
  bool usuarioEncontrado = false;
  bool refreshNeeded = true; // Control de refresco
  unsigned long lastRefresh = 0;

  while(true) {
    // Control de refresco (solo cada 100ms)
    if(millis() - lastRefresh > 100 && refreshNeeded) {
      lcd.clear();
      
      if(!modoConfirmacion) {
        // Buscar usuario registrado
        usuarioEncontrado = false;
        for(uint8_t id = currentID; id <= 127; id++) {
          if(finger.loadModel(id) == FINGERPRINT_OK) {
            currentID = id;
            usuarioEncontrado = true;
            break;
          }
        }
        
        if(!usuarioEncontrado) {
          lcd.setCursor(0, 1);
          lcd.print("No hay más usuarios");
          delay(1500);
          return;
        }

        // Mostrar info compacta (sin etiquetas redundantes)
        lcd.setCursor(0, 0);
        lcd.print("ID:" + String(currentID));
        
        String nombre = obtenerNombrePorID(currentID);
        lcd.setCursor(0, 1);
        lcd.print(nombre);
        
        String curso = obtenerCursoPorID(currentID);
        lcd.setCursor(0, 2);
        lcd.print(curso);
        
        lcd.setCursor(0, 3);
        lcd.print("Sel:Elim    ▲/▼");
      } else {
        // Modo confirmación
        lcd.setCursor(0, 0);
        lcd.print("¿Eliminar este usuario?");
        lcd.setCursor(0, 1);
        lcd.print("ID:" + String(currentID));
        lcd.setCursor(0, 2);
        lcd.print(obtenerNombrePorID(currentID));
        lcd.setCursor(0, 3);
        lcd.print("▲:Si ▼:No");
      }
      
      refreshNeeded = false;
      lastRefresh = millis();
    }

    // Navegación mejorada sin parpadeos
    if(digitalRead(UP_PIN) == LOW && digitalRead(DOWN_PIN) == LOW) {
      delay(300); // Debounce
      return; // Salir al menú principal
    }
    else if(digitalRead(UP_PIN) == LOW) {
      delay(150);
      if(modoConfirmacion) {
        // Confirmar eliminación
        if(finger.deleteModel(currentID) == FINGERPRINT_OK) {
          eliminarDelJSON(currentID);
          lcd.clear();
          lcd.setCursor(4, 1);
          lcd.print("¡Eliminado!");
          beep(true);
          delay(1000);
          currentID++;
        }
        modoConfirmacion = false;
      } else {
        currentID++;
      }
      refreshNeeded = true;
    }
    else if(digitalRead(DOWN_PIN) == LOW) {
      delay(150);
      if(modoConfirmacion) {
        modoConfirmacion = false;
      } else {
        if(currentID > 1) currentID--;
      }
      refreshNeeded = true;
    }
    else if(digitalRead(SELECT_PIN) == LOW && !modoConfirmacion && usuarioEncontrado) {
      delay(150);
      modoConfirmacion = true;
      refreshNeeded = true;
    }

    delay(50); // Pequeña pausa para evitar sobrecarga
  }
}
void modoAsistencia() {
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print("Modo Asistencia");
  delay(1000);

  while(digitalRead(DOWN_PIN) != LOW) {
    lcd.setCursor(0, 1);
    lcd.print("Coloca dedo...");
    lcd.setCursor(3, 3);
    lcd.print("Volver: Abajo");

    while(finger.getImage() != FINGERPRINT_OK) {
      if(digitalRead(DOWN_PIN) == LOW) return;
      delay(100);
    }

    String fecha = obtenerFechaHoraActual().substring(0, 10);
    String hora = obtenerFechaHoraActual().substring(11);
    String archivoCSV = "/asistencia-" + fecha + ".csv";

    if(finger.image2Tz() != FINGERPRINT_OK) {
      lcd.setCursor(0, 2);
      lcd.print("Huella no legible");
      beep(false);

      File logError = SD.open(archivoCSV, FILE_APPEND);
      if(logError) {
        logError.println("ERROR,Huella no legible," + fecha + " " + hora);
        logError.close();
      }

      delay(1500);
      lcd.clear();
      continue;
    }

    if(finger.fingerFastSearch() != FINGERPRINT_OK) {
      lcd.setCursor(0, 2);
      lcd.print("No encontrada");
      beep(false);

      File logError = SD.open(archivoCSV, FILE_APPEND);
      if(logError) {
        logError.println("ERROR,Huella no reconocida," + fecha + " " + hora);
        logError.close();
      }

      delay(1500);
      lcd.clear();
      continue;
    }

    uint8_t idDetectado = finger.fingerID;
    String nombre = obtenerNombrePorID(idDetectado);
    String curso = obtenerCursoPorID(idDetectado);

    bool yaRegistrado = false;
    File asistencia = SD.open(archivoCSV);
    if(asistencia) {
      while(asistencia.available()) {
        String linea = asistencia.readStringUntil('\n');
        if(linea.startsWith(String(idDetectado) + ",")) {
          yaRegistrado = true;
          break;
        }
      }
      asistencia.close();
    }

    if(yaRegistrado) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Ya registraste hoy");
      beep(false);

      File logError = SD.open(archivoCSV, FILE_APPEND);
      if(logError) {
        logError.println("ERROR,Intento duplicado de ID " + String(idDetectado) + "," + fecha + " " + hora);
        logError.close();
      }

      delay(2000);
      lcd.clear();
      continue;
    }

    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("ID: "); lcd.print(idDetectado);
    lcd.setCursor(0, 1); lcd.print("Nombre: "); lcd.print(nombre);
    lcd.setCursor(0, 2); lcd.print("Curso: "); lcd.print(curso);
    lcd.setCursor(0, 3); lcd.print("Hora: "); lcd.print(hora);
    beep(true);

    asistencia = SD.open(archivoCSV, FILE_APPEND);
    if(asistencia) {
      asistencia.println(String(idDetectado) + "," + nombre + "," + curso + "," + fecha + " " + hora);
      asistencia.close();
    }

    delay(3000);
    lcd.clear();
  }
}

void modoVerArchivos() {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("Archivos en SD");
  delay(1000);

  File root = SD.open("/");
  String archivos[50];
  int total = 0;

  while(true) {
    File entry = root.openNextFile();
    if(!entry) break;

    if(!entry.isDirectory()) {
      archivos[total++] = entry.name();
      if(total >= 50) break;
    }
    entry.close();
  }

  int index = 0;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Archivo:");
  lcd.setCursor(0, 1);
  lcd.print(archivos[index]);

  while(true) {
    if(digitalRead(UP_PIN) == LOW) {
      index = (index - 1 + total) % total;
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(archivos[index]);
      delay(300);
    }

    if(digitalRead(DOWN_PIN) == LOW) {
      index = (index + 1) % total;
      lcd.setCursor(0, 1);
      lcd.print("                ");
      lcd.setCursor(0, 1);
      lcd.print(archivos[index]);
      delay(300);
    }

    if(digitalRead(SELECT_PIN) == LOW) {
      lcd.clear();
      lcd.setCursor(5, 0);
      lcd.print("Volviendo...");
      delay(1000);
      return;
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(UP_PIN, INPUT_PULLUP);
  pinMode(DOWN_PIN, INPUT_PULLUP);
  pinMode(SELECT_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Inicializando...");
  delay(1000);

  if(!rtc.begin()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error RTC");
    while(1);
  }

  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  delay(100);
  finger.begin(57600);

  if(!SD.begin(SD_CS)) {
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Error SD");
    while(1);
  }

  if(!finger.verifyPassword()) {
    lcd.clear();
    lcd.setCursor(0, 2);
    lcd.print("Error Huella");
    while(1);
  }

  // Ajustar hora solo una vez
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sistema Biometrico");
  lcd.setCursor(7, 1);
  lcd.print("Magis");
  lcd.setCursor(0, 3);
  lcd.print("By FarfanDev");
  beep(true);
  delay(1000);

}

// Modificación en el loop()
void loop() {
  mostrarMenu();
  
  static unsigned long ultimaAccion = 0;
  if(millis() - ultimaAccion > 200) {
    if(digitalRead(UP_PIN) == LOW) {
      opcionSeleccionada = (opcionSeleccionada > 0) ? opcionSeleccionada - 1 : 3;
      ultimaAccion = millis();
    }
    else if(digitalRead(DOWN_PIN) == LOW) {
      opcionSeleccionada = (opcionSeleccionada < 3) ? opcionSeleccionada + 1 : 0;
      ultimaAccion = millis();
    }
    else if(digitalRead(SELECT_PIN) == LOW) {
      ultimaAccion = millis();
      lcd.clear();
      switch(opcionSeleccionada) {
        case 0: modoRegistro(); break;
        case 1: modoAsistencia(); break;
        case 2: modoEliminarUsuario(); break;
        case 3: modoVerArchivos(); break;
      }
      ultimaOpcion = 255;
    }
  }
}