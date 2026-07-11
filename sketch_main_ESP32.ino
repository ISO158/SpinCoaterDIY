#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// ==========================================
// --- DEFINIÇÕES DE HARDWARE ---
// ==========================================

// --- PINAGEM DO MOTOR ---
const int escPin = 18;
const int pinTelemetria = 19;
const int PARES_DE_POLOS = 7;

// --- PINAGEM DOS LEDS ---
#define PIN_LED_VERMELHO 16 
#define PIN_LED_VERDE    17 

// --- PINAGEM DO ENCODER ---
#define ENC_CLK 25  
#define ENC_DT  33  
#define ENC_SW  32  

// ==========================================
// --- CONFIGURAÇÕES GERAIS ---
// ==========================================
#define INTERVALO_DISPLAY 500  
#define NUM_AMOSTRAS 50        
#define PWM_MIN 1055           
#define PWM_MAX 2000

#define PASSO_FREIO 4          
#define TEMPO_FREIO 20         

// ZONA DE CORTE (Hard Cut para não travar no fim)
#define PWM_CORTE_INERCIA 1130 

// Rampa de Aceleração (Tempo total em ms)
#define DURACAO_RAMPA 2000 

// --- PLATÔS SEGUROS (BAIXA ROTAÇÃO) ---
const int degrausSeguros[] = {
  570, 880, 1150, 1400, 1650, 1900, 2150, 2400, 2650, 
  2900, 3150, 3400, 3650, 3900, 4150, 4400, 4650 
};
const int numDegraus = sizeof(degrausSeguros) / sizeof(degrausSeguros[0]);

// A partir daqui, ativamos a CORREÇÃO DE CARGA
const int LIMITE_LINEAR = 4650; 

// ==========================================
// --- VARIÁVEIS GLOBAIS ---
// ==========================================
Servo esc;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

int cfgDepositRPM = 570; 
int cfgMainRPM = 2150;    
int cfgTime = 30;         

long rpmMedio = 0;        
float voltagem = 0.0;     
float temperatura = 0.0;      

long amostrasRPM[NUM_AMOSTRAS]; 
int indiceAmostra = 0;
long somaAmostras = 0;

// Controle Motor
float pwmAtualFloat = 1000.0; 
int pwmAlvo = 1000;       
int pwmInicialRampa = 1000;
unsigned long inicioRampa = 0;
bool rampaAtiva = false;

// Compensador de Carga
unsigned long ultimoAjusteCarga = 0;
#define INTERVALO_COMPENSACAO 200 // Verifica a cada 200ms
#define TOLERANCIA_CARGA 100      // Só corrige se cair mais que 100 RPM

unsigned long tempoInicioSpin = 0;
unsigned long ultimaAtualizacaoDisplay = 0; 
unsigned long ultimoPassoFreio = 0; 

enum Estado { TELA_HOME, TELA_CONFIG, TELA_PRE_RUN, MODO_DEPOSICAO, MODO_SPIN, MODO_FREIO, TELA_FIM };
Estado estadoAtual = TELA_HOME;

int cursorLinha = 0;
int cursorPreRun = 0; 
bool botaoPressionado = false;
unsigned long ultimoClick = 0;

volatile int encoderCount = 0;
int lastEncoderCount = 0;

int indexDeposit = 0; 
int indexMain = 6;    

// ==========================================
// --- FUNÇÕES AUXILIARES ---
// ==========================================

// --- ENCODER E CRC ---
void IRAM_ATTR isrEncoder() {
  static unsigned long lastInterruptTime = 0;
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime > 5) { 
    if (digitalRead(ENC_CLK) == digitalRead(ENC_DT)) {
      encoderCount--;
    } else {
      encoderCount++;
    }
    lastInterruptTime = interruptTime;
  }
}

uint8_t updateCrc8(uint8_t crc, uint8_t crc_seed) {
  uint8_t crc_u = crc ^ crc_seed;
  for (int i = 0; i < 8; i++) crc_u = (crc_u & 0x80) ? 0x7 ^ (crc_u << 1) : (crc_u << 1);
  return crc_u;
}

// --- CALIBRAÇÃO LINEAR ---
int rpmToPwmEstimado(int rpm) {
  if (rpm < 100) return 1000;
  float pwm = (0.0795 * rpm) + 1031;
  if (pwm < PWM_MIN) pwm = PWM_MIN;
  if (pwm > PWM_MAX) pwm = PWM_MAX;
  return (int)pwm;
}

// --- VISUAL ---
void desenharOpcaoInvertida(String texto, int y, bool selecionado) {
  int x = (128 - (texto.length() * 6)) / 2;
  if (selecionado) {
    display.fillRect(x - 2, y - 1, (texto.length() * 6) + 4, 10, WHITE);
    display.setTextColor(BLACK, WHITE);
  } else {
    display.setTextColor(WHITE);
  }
  display.setCursor(x, y); display.print(texto); display.setTextColor(WHITE);
}

void desenharBotaoGrande(String texto, int y, bool selecionado) {
  int larguraPx = texto.length() * 12;
  int x = (128 - larguraPx) / 2;
  display.setTextSize(2);
  if (selecionado) {
    display.fillRect(x - 4, y - 2, larguraPx + 8, 18, WHITE);
    display.setTextColor(BLACK, WHITE);
  } else {
    display.setTextColor(WHITE);
  }
  display.setCursor(x, y); display.print(texto); display.setTextColor(WHITE); display.setTextSize(1);
}

void desenharLinhaConfig(String label, String valor, String unidade, int y, bool selecionado) {
  if (selecionado) {
    display.fillRect(2, y - 1, 124, 10, WHITE);
    display.setTextColor(BLACK, WHITE);
  } else {
    display.setTextColor(WHITE);
  }
  display.setCursor(4, y); display.print(label);
  display.setCursor(65, y); display.print(valor); display.print(" "); display.print(unidade);
  display.setTextColor(WHITE);
}

void desenharDashboard(long rpmReal, int tempoTotal, int tempoRestante, int modo) {
  display.setTextSize(1);
  display.setCursor(4, 4); display.print(voltagem, 1); display.print(" V");
  display.setCursor(90, 4); display.print(temperatura, 1); display.print(" C");

  display.setTextSize(2);
  String sRpm = String(rpmReal);
  int xRpm = (128 - (sRpm.length() * 12)) / 2;
  xRpm -= 10; 
  display.setCursor(xRpm, 22); display.print(sRpm);
  display.setTextSize(1);
  display.setCursor(xRpm + (sRpm.length() * 12) + 2, 29); display.print("RPM");

  if (modo == 1 && tempoTotal > 0) { 
     display.drawRect(14, 45, 100, 6, WHITE);
     int tempoCorrido = tempoTotal - tempoRestante;
     int larguraBarra = map(tempoCorrido, 0, tempoTotal, 0, 96);
     if (larguraBarra > 96) larguraBarra = 96;
     display.fillRect(16, 47, larguraBarra, 2, WHITE);
     String sTempo = String(tempoRestante) + " s";
     int xTempo = (128 - (sTempo.length()*6))/2;
     display.setCursor(xTempo, 54); display.print(sTempo);
  }
  else if (modo == 2) { 
      display.setTextSize(1);
      desenharOpcaoInvertida("PARANDO...", 48, true);
  }
}

// ==========================================
// --- SETUP ---
// ==========================================
void setup() {
  Serial.begin(115200);

  // 1. INICIALIZAÇÃO IMEDIATA DO MOTOR (Evita bipes de erro)
  esc.setPeriodHertz(50);
  esc.attach(escPin, 1000, 2000);
  esc.writeMicroseconds(1000); // Crava o pino em 1000us na largada
  
  // O resto da placa continua inicializando enquanto o ESC lê o sinal zero
  pinMode(PIN_LED_VERMELHO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  
  digitalWrite(PIN_LED_VERMELHO, HIGH); delay(200); digitalWrite(PIN_LED_VERMELHO, LOW);
  digitalWrite(PIN_LED_VERDE, HIGH); delay(200); digitalWrite(PIN_LED_VERDE, LOW);

  for(int i=0; i<NUM_AMOSTRAS; i++) amostrasRPM[i] = 0;

  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), isrEncoder, RISING);

  // 2. INICIALIZA O DISPLAY
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { for(;;); }
  display.setTextColor(WHITE);
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10,30); display.println("Booting System...");
  display.display();
  
  // 3. DELAY DE ARME DO ESC (A tela fica estática por 2s enquanto o motor bipa)
  delay(2000); 

  // 4. INICIALIZA A TELEMETRIA
  Serial2.begin(115200, SERIAL_8N1, pinTelemetria, -1);
}

void gerenciarLeds() {
  if (estadoAtual == MODO_DEPOSICAO || estadoAtual == MODO_SPIN || estadoAtual == MODO_FREIO) {
    digitalWrite(PIN_LED_VERMELHO, HIGH); 
    digitalWrite(PIN_LED_VERDE, LOW);
  } else {
    digitalWrite(PIN_LED_VERMELHO, LOW);
    digitalWrite(PIN_LED_VERDE, HIGH);    
  }
}

// ==========================================
// --- LÓGICA DE CONTROLE (RAMPA E CARGA) ---
// ==========================================
void iniciarNovaRampa(int novoAlvo) {
  pwmAlvo = novoAlvo;

  // --- KICKSTART PARA VENCER A INÉRCIA DO CHUCK ---
  // Se o motor estava desligado e mandamos ligar, dá um tranco de 150ms
  if (pwmAtualFloat < 1050 && novoAlvo > 1000) {
    esc.writeMicroseconds(1150); // PWM mais forte para o motor dar um "pulo"
    delay(150);                  // Tempo suficiente só para vencer a inércia
  }
  // ------------------------------------------------------

  // Jump Start para baixas velocidades (ignora rampa)
  if (pwmAlvo <= PWM_CORTE_INERCIA) {
    pwmAtualFloat = pwmAlvo;
    rampaAtiva = false; 
    esc.writeMicroseconds(pwmAlvo);
    return;
  }

  pwmInicialRampa = (int)pwmAtualFloat; 
  if (pwmInicialRampa < 1000) pwmInicialRampa = 1000;
  inicioRampa = millis();
  rampaAtiva = true;
}

void atualizarRampa() {
  if (!rampaAtiva) return;

  unsigned long tempoDecorrido = millis() - inicioRampa;

  if (tempoDecorrido >= DURACAO_RAMPA) {
    pwmAtualFloat = pwmAlvo;
    rampaAtiva = false;
  } 
  else {
    float progresso = (float)tempoDecorrido / (float)DURACAO_RAMPA;
    pwmAtualFloat = pwmInicialRampa + ((pwmAlvo - pwmInicialRampa) * progresso);
  }
  esc.writeMicroseconds((int)pwmAtualFloat);
}

// --- COMPENSADOR DE CARGA (Só p/ Alta Rotação) ---
void compensarCarga() {
  if (!rampaAtiva && pwmAlvo > rpmToPwmEstimado(LIMITE_LINEAR)) {
    if (millis() - ultimoAjusteCarga > INTERVALO_COMPENSACAO) {
      
      long erro = cfgMainRPM - rpmMedio;

      if (erro > TOLERANCIA_CARGA) {
         pwmAtualFloat += 1.0; 
         if (pwmAtualFloat > PWM_MAX) pwmAtualFloat = PWM_MAX;
         esc.writeMicroseconds((int)pwmAtualFloat);
      }
      else if (erro < -TOLERANCIA_CARGA) {
         pwmAtualFloat -= 1.0;
         if (pwmAtualFloat < PWM_MIN) pwmAtualFloat = PWM_MIN;
         esc.writeMicroseconds((int)pwmAtualFloat);
      }
      ultimoAjusteCarga = millis();
    }
  }
}

// ==========================================
// --- LOOP PRINCIPAL ---
// ==========================================
void loop() {
  processarTelemetria();
  gerenciarLeds();

  if (digitalRead(ENC_SW) == LOW) {
    if (millis() - ultimoClick > 300) {
      botaoPressionado = true;
      ultimoClick = millis();
    }
  }

  switch (estadoAtual) {
    case TELA_HOME:      loopHome();      break;
    case TELA_CONFIG:    loopConfig();    break;
    case TELA_PRE_RUN:   loopPreRun();    break;
    case MODO_DEPOSICAO: loopDeposicao(); break;
    case MODO_SPIN:      loopSpin();      break;
    case MODO_FREIO:     loopFreio();     break;
    case TELA_FIM:       loopFim();       break;
  }
}

void processarTelemetria() {
  while (Serial2.available() >= 10) {
    uint8_t buf[10];
    if (Serial2.peek() > 150) { Serial2.read(); continue; }
    Serial2.readBytes(buf, 10);
    uint8_t crc = 0;
    for (int i = 0; i < 9; i++) crc = updateCrc8(crc, buf[i]);
    if (crc == buf[9]) {
      uint16_t rpmWord = (buf[7] << 8) | buf[8];
      long leituraInstantanea = (long(rpmWord) * 100) / PARES_DE_POLOS;
      somaAmostras -= amostrasRPM[indiceAmostra]; 
      amostrasRPM[indiceAmostra] = leituraInstantanea; 
      somaAmostras += leituraInstantanea; 
      indiceAmostra++;
      if (indiceAmostra >= NUM_AMOSTRAS) indiceAmostra = 0; 
      rpmMedio = somaAmostras / NUM_AMOSTRAS;
      temperatura = buf[0];
      uint16_t voltageWord = (buf[1] << 8) | buf[2];
      voltagem = (float)voltageWord / 100.0;
    }
  }
}

bool horaDeAtualizarTela() {
  if (millis() - ultimaAtualizacaoDisplay > INTERVALO_DISPLAY) {
    ultimaAtualizacaoDisplay = millis();
    return true;
  }
  return false;
}

// ==========================================
// --- MÁQUINAS DE ESTADO ---
// ==========================================

void loopHome() {
  esc.writeMicroseconds(1000);
  pwmAtualFloat = 1000;
  rampaAtiva = false;
  if (botaoPressionado) {
    botaoPressionado = false;
    cursorLinha = 0; 
    cursorPreRun = 0;
    estadoAtual = TELA_CONFIG;
    display.clearDisplay(); 
    return;
  }
  if (horaDeAtualizarTela()) {
      display.clearDisplay();
      display.drawRect(0, 0, 128, 64, WHITE);
      display.setTextSize(1);
      display.setCursor(31,6); display.println("SPIN COATER");
      desenharBotaoGrande("INICIAR", 25, true); 
      display.setTextSize(1);
      display.setCursor(43, 52); display.println("by Igor");
      display.display();
  }
}

void loopConfig() {
  bool mudouAlgo = false;
  if (encoderCount != lastEncoderCount) {
    int delta = encoderCount - lastEncoderCount;
    mudouAlgo = true;
    
    if (cursorLinha == 0) { 
       if (cfgDepositRPM < LIMITE_LINEAR) {
          if (delta > 0) indexDeposit++; else indexDeposit--;
          if (indexDeposit < 0) indexDeposit = 0;
          if (indexDeposit >= numDegraus) indexDeposit = numDegraus - 1;
          cfgDepositRPM = degrausSeguros[indexDeposit];
       } else {
          cfgDepositRPM += delta * 100;
          if (cfgDepositRPM < LIMITE_LINEAR) {
             indexDeposit = numDegraus - 2; 
             cfgDepositRPM = degrausSeguros[indexDeposit];
          }
       }
       if (cfgDepositRPM > cfgMainRPM) cfgMainRPM = cfgDepositRPM;
    }
    else if (cursorLinha == 1) { 
       if (cfgMainRPM < LIMITE_LINEAR) {
          if (delta > 0) indexMain++; else indexMain--;
          if (indexMain < 0) indexMain = 0;
          if (indexMain >= numDegraus) indexMain = numDegraus - 1;
          cfgMainRPM = degrausSeguros[indexMain];
       } else {
          cfgMainRPM += delta * 100;
          if (cfgMainRPM < LIMITE_LINEAR) {
             indexMain = numDegraus - 2;
             cfgMainRPM = degrausSeguros[indexMain];
          }
       }
       if (cfgMainRPM < cfgDepositRPM) cfgMainRPM = cfgDepositRPM;
       if (cfgMainRPM > 12000) cfgMainRPM = 12000;
    }
    else if (cursorLinha == 2) { 
       cfgTime += delta;
       if (cfgTime < 1) cfgTime = 1;
       if (cfgTime > 600) cfgTime = 600;
    }
    lastEncoderCount = encoderCount;
  }

  if (botaoPressionado) {
    botaoPressionado = false;
    mudouAlgo = true;
    cursorLinha++;
    if (cursorLinha > 2) { 
       cursorLinha = 0;
       estadoAtual = TELA_PRE_RUN;
       display.clearDisplay(); 
       return;
    }
  }

  if (mudouAlgo || horaDeAtualizarTela()) {
      display.clearDisplay();
      display.drawRect(0, 0, 128, 64, WHITE);
      desenharLinhaConfig("Deposit", String(cfgDepositRPM), "RPM", 10, (cursorLinha==0));
      desenharLinhaConfig("Speed",   String(cfgMainRPM),    "RPM", 26, (cursorLinha==1));
      desenharLinhaConfig("Time",    String(cfgTime),       "s",   42, (cursorLinha==2));
      display.display();
  }
}

void loopPreRun() {
  bool mudou = false;
  if (encoderCount != lastEncoderCount) {
     int delta = encoderCount - lastEncoderCount;
     if (delta > 0) cursorPreRun++; else cursorPreRun--;            
     if (cursorPreRun < 0) cursorPreRun = 0; 
     if (cursorPreRun > 2) cursorPreRun = 2; 
     lastEncoderCount = encoderCount;
     mudou = true;
  }
  if (botaoPressionado) {
    botaoPressionado = false;
    if (cursorPreRun == 0) estadoAtual = MODO_DEPOSICAO;
    else if (cursorPreRun == 1) estadoAtual = TELA_CONFIG;
    else if (cursorPreRun == 2) estadoAtual = TELA_HOME;
    display.clearDisplay();
    return;
  }
  if (mudou || horaDeAtualizarTela()) {
      display.clearDisplay();
      display.drawRect(0, 0, 128, 64, WHITE);
      display.setTextSize(1);
      desenharOpcaoInvertida("INICIAR", 15, (cursorPreRun == 0));
      desenharOpcaoInvertida("EDITAR",  30, (cursorPreRun == 1));
      desenharOpcaoInvertida("SAIR",    45, (cursorPreRun == 2));
      display.display();
  }
}

void loopDeposicao() {
  if (!rampaAtiva && pwmAlvo != rpmToPwmEstimado(cfgDepositRPM)) {
     iniciarNovaRampa(rpmToPwmEstimado(cfgDepositRPM));
  }
  
  atualizarRampa();
  
  if (cfgDepositRPM > LIMITE_LINEAR) compensarCarga();

  if (botaoPressionado) {
    botaoPressionado = false;
    tempoInicioSpin = millis();
    iniciarNovaRampa(rpmToPwmEstimado(cfgMainRPM)); 
    estadoAtual = MODO_SPIN;
    display.clearDisplay();
    return;
  }
  if (horaDeAtualizarTela()) {
      display.clearDisplay();
      display.drawRect(0, 0, 128, 64, WHITE);
      desenharDashboard(rpmMedio, 0, 0, 0);
      desenharOpcaoInvertida("ACELERAR", 52, true);
      display.display();
  }
}

void loopSpin() {
  unsigned long tempoCorrido = (millis() - tempoInicioSpin) / 1000;
  long tempoRestante = cfgTime - tempoCorrido;

  if (tempoRestante <= 0) {
    estadoAtual = MODO_FREIO;
    display.clearDisplay();
    return;
  }

  atualizarRampa();
  
  if (cfgMainRPM > LIMITE_LINEAR) compensarCarga();

  if (botaoPressionado) {
    botaoPressionado = false;
    estadoAtual = MODO_FREIO;
    display.clearDisplay();
    return;
  }
  if (horaDeAtualizarTela()) {
      display.clearDisplay();
      display.drawRect(0, 0, 128, 64, WHITE);
      desenharDashboard(rpmMedio, cfgTime, tempoRestante, 1);
      display.display();
  }
}

void loopFreio() {
  if (millis() - ultimoPassoFreio > TEMPO_FREIO) {
    pwmAtualFloat -= PASSO_FREIO; 
    
    // HARD CUT: Zona de instabilidade
    if (pwmAtualFloat < PWM_CORTE_INERCIA) {
      pwmAtualFloat = 1000;
      esc.writeMicroseconds(1000);
      delay(200); 
      estadoAtual = TELA_FIM; 
      return;
    }
    esc.writeMicroseconds((int)pwmAtualFloat);
    ultimoPassoFreio = millis();
  }
  if (horaDeAtualizarTela()) {
      display.clearDisplay();
      display.drawRect(0, 0, 128, 64, WHITE);
      desenharDashboard(rpmMedio, 0, 0, 2);
      display.display();
  }
}

void loopFim() {
  esc.writeMicroseconds(1000); 
  pwmAtualFloat = 1000;
  rampaAtiva = false;
  if (botaoPressionado) {
    botaoPressionado = false;
    estadoAtual = TELA_HOME;
    display.clearDisplay();
    return;
  }
  if (horaDeAtualizarTela()) {
      display.clearDisplay();
      display.drawRect(0, 0, 128, 64, WHITE);
      display.setTextSize(2);
      display.setCursor(10,20); display.println("CONCLUIDO");
      display.setTextSize(1);
      desenharOpcaoInvertida("REINICIAR", 45, true);
      display.display();
  }
}
