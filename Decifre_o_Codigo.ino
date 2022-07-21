//Eletrogate
//adaptado de: Michael Klements, The DIY Life

//Rotina de interrupção do encoder adaptada do código de exemplo de Simon Merrett

#include <SPI.h>                          //Importando bibliotecas para controlar o display OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>                        //Biblioteca de importação para controlar o servo

Servo lockServo;                          ////Cria um objeto servo chamado lockServo

#define SCREEN_WIDTH 128                  // OLED display largura, em pixels
#define SCREEN_HEIGHT 32                  // OLED display altura, em pixels

#define OLED_RESET -1                     // Pino de reset
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);   //Criando o objeto para controlar o display
/*
  void PinA();
  void PinB();
  void startupAni ();
  void updateDisplayCode();
  void updateLEDs (int corNum, int corPla);
  void checkCodeGuess();
  void inputCodeGuess();
  void generateNewCode();
*/
static int pinA = 2;                      //Pino para interrupção interna do Enconder
static int pinB = 3;                      //Pino para interrupção interna do Enconder
volatile byte aFlag = 0;                  //PinA detectando apenas a borda de subida
volatile byte bFlag = 0;                  //PinB como borda de subida na detecção
volatile byte encoderPos = 0;             //Posição atual do Encoder, que pode variar de 0 a 9
volatile byte prevEncoderPos = 0;         //Variavel para armazenar o valor anterior, para saber se o encoder foi girado.
volatile byte reading = 0;                //Armazena o valor dos pinos de interrupção

const byte buttonPin = 4;                 //Pino do botão do encoder
byte oldButtonState = HIGH;               //Armazena o antigo estado do botão
const unsigned long debounceTime = 10;    //Delay para evitar erros na leitura do botão
unsigned long buttonPressTime;            //Tempo que o botão foi pressionado para debounce

byte correctNumLEDs[4] = {6, 8, 10, 12};   //Pinos para LEDs de número correto (indicam um dígito correto)
byte correctPlaceLEDs[4] = {7, 9, 11, 13}; //Pinos para LEDs no local correto (indicam um dígito correto no local correto)

byte code[4] = {0, 0, 0, 0};               //Cria um vetor para armazenar os dígitos do código a ser decifrado
byte codeGuess[4] = {0, 0, 0, 0};          //Cria um vetor de código digitado pelo usuário
byte guessingDigit = 0;                    //Variável para armazenar o digito atual do usuário
byte numGuesses = 0;                       //Variável que armazena a quantidade de tentativas
boolean correctGuess = true;               //Variável para verificar se o código foi adivinhado corretamente, verdadeiro inicialmente para gerar um novo código na inicialização


void setup()
{
  Serial.begin(9600);                                 //Inicia o Monitor Seial para realização de eventuais debugging
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))     //Inicia o display
  {
    Serial.println(F("SSD1306 a inicialização falhou"));   //Se a conexão falhou
    for (;;);                                         //Loop infinito para não continuar
  }
  display.clearDisplay();                             //A inicialização do display foi bem sucedida, então limpa o display
  lockServo.attach(5);                                //Inicia o servo no pino 5
  for (int i = 0 ; i <= 3 ; i++)                      //Define todos os LEDs como saída
  {
    pinMode(correctNumLEDs[i], OUTPUT);
    pinMode(correctPlaceLEDs[i], OUTPUT);
  }
  pinMode(pinA, INPUT_PULLUP);                        //Seta PinA como entrada PULLUP,para não precisar do resistor.
  pinMode(pinB, INPUT_PULLUP);                        //Seta PinB como entrada PULLUP,para não precisar do resistor.
  attachInterrupt(0, PinA, RISING);                   //Inicia a rotina de interrupção dos pinos de rotação do encoder
  attachInterrupt(1, PinB, RISING);
  pinMode (buttonPin, INPUT_PULLUP);                  //Seta o botão como entrada PULLUP,para não precisar do resistor.
  randomSeed(analogRead(0));                          //Função para gerar números aleatórios
  display.setTextColor(SSD1306_WHITE);                //Setando a cor do texto para branco
  startupAni();                                       //Inicia a Animação
}

void loop()
{
  if (correctGuess)                                           //Se o codigo está correto, destrava e gera um novo código.
  {
    lockServo.write(140);                                     //Destrava o cofre com o servo
    delay(300);
    updateLEDs (0, 4);                                        //Piscando a sequencia de LEDS
    delay(300);
    updateLEDs (4, 0);
    delay(300);
    updateLEDs (0, 4);
    delay(300);
    updateLEDs (4, 0);
    delay(300);
    updateLEDs (4, 4);                                        //Desliga todos os LEDs
    if (numGuesses >= 1)                                      //Verifica se não é o inicio do jogo para dar as informações do jogo
    {
      display.clearDisplay();                                 //limpa  o display
      display.setTextSize(1);                                 //Seta o tamanho da fonte menor
      display.setCursor(35, 10);                              //Seta o posição do texto
      display.print(F("Em "));                                //Imprime as informações na tela
      display.print(numGuesses);
      display.setCursor(35, 20);                              //Seta o posição do texto
      display.print(F("Tentativas"));                         //Imprime as informações na tela
      display.display();                                      //Exibe o texto
      delay(5000);
    }
    display.clearDisplay();                                   //limpa  o display
    display.setTextSize(1);                                   //Seta o tamanho da fonte
    display.setCursor(25, 10);                                //Seta o posição do texto
    display.print(F("Aperte para"));                              //Imprime as informações na tela
    display.setCursor(15, 20);                                //Seta o posição do texto
    display.print(F("trancar o cofre"));                            //Imprime as informações na tela
    display.display();                                        //Exibe o texto
    display.setTextSize(2);                                   //Imprime as informações na tela size back to large
    boolean lock = false;                                     //O cofre não esta bloqueado inicialmente
    boolean pressed = false;                                  //Mantém o controle do pressionamento do botão
    while (!lock)                                             //Enquanto o botão não está pressionado, aguarde até que seja pressionado
    {
      byte buttonState = digitalRead (buttonPin);
      if (buttonState != oldButtonState)
      {
        if (millis () - buttonPressTime >= debounceTime)      //Debounce botão
        {
          buttonPressTime = millis ();                        //Tempo enquanto o botão é pressionado
          oldButtonState =  buttonState;                      //Lembrar o ultimo estado do botão
          if (buttonState == LOW)
          {
            pressed = true;                                   //Lembrar que o botão foi pressionado
          }
          else
          {
            if (pressed == true)                              //Certifica de que o botão seja pressionado e depois liberado antes de continuar no código
            {
              lockServo.write(45);                            //Tranca o cofre
              display.clearDisplay();                         //Limpa o display
              display.setCursor(15, 10);                      //Seta o posição do texto
              display.print(F("Trancado"));                   //Imprime as informações na tela
              display.display();                              //Exibe o texto
              lock = true;
            }
          }
        }
      }
    }
    generateNewCode();                                        //Chama a função para gerar novo código aleatório
    updateLEDs (0, 0);
    correctGuess = false;                                     //A variavel que monitora se o codigo está correto inicia como falso
    numGuesses = 0;                                           //Reseta o número de tentativas
  }
  inputCodeGuess();                                           //Chama a função para que o usuário digite um novo código
  numGuesses++;                                               //Aumenta em 1 o número de tentativas
  checkCodeGuess();                                           //Chama a função que checa se o código está correto
  encoderPos = 0;                                             //Reseta a posição do encoder
  guessingDigit = 0;                                          //Reseta o digito a ser testado para zero
  codeGuess[0] = 0;                                           //Reseta o primeiro digito do codigo para 0
  updateDisplayCode();                                        //Atualiza o código exibido no display
}

void updateDisplayCode()                                      //Função para atualizar a tela com o código
{
  String temp = "";                                           //Variável temporária para concatenar a string de código
  if (!correctGuess)                                          //Se a suposição não estiver correta, atualize a tela
  {
    for (int i = 0 ; i < guessingDigit ; i++)                 //Percorre os quatro dígitos para exibi-los
    {
      temp = temp + codeGuess[i];
    }
    temp = temp + encoderPos;
    for (int i = guessingDigit + 1 ; i <= 3 ; i++)
    {
      temp = temp + "0";
    }
    display.setTextSize(2);                                   //Imprime as informações na tela size
    display.clearDisplay();                                   //Limpa o display
    display.setCursor(40, 10);                                //Seta o posição do texto
    display.println(temp);                                    //Imprime as informações na tela
    display.display();                                        //Update the display
  }
}

void generateNewCode()                                        //Função para gerar um novo código aleatório
{
  Serial.print("Code: ");
  for (int i = 0 ; i <= 3 ; i++)                              //Percorre os quatro dígitos e atribui um número aleatório a cada um
  {
    code[i] = random(0, 9);                                   //Gera um numero aleatório para cada digito
    Serial.print(code[i]);                                    //Exibir o código no monitor serial para depuração

  }
  Serial.println();
}

void inputCodeGuess()                                         //Função que permite ao usuário inserir uma tentativa
{
  //  byte buttonState;
  for (int i = 0 ; i <= 3 ; i++)                              //O usuário deve adivinhar todos os quatro dígitos
  {
    guessingDigit = i;
    boolean confirmed = false;                                //Ambos usados para confirmar o pressionamento do botão para atribuir um dígito ao código a ser advinhado
    boolean pressed = false;
    encoderPos = 0;                                           //O encoder começa em 0 para cada dígito
    while (!confirmed)                                        //Aguarda enquanto o usuário não confirmar o digito
    {
      byte buttonState = digitalRead (buttonPin);
      if (buttonState != oldButtonState)
      {
        if (millis () - buttonPressTime >= debounceTime)      //Botão debounce
        {
          buttonPressTime = millis ();                        //Registra o tempo em que o botão foi pressionado
          oldButtonState =  buttonState;                      //Registra o antigo estado do botão
          if (buttonState == LOW)
          {
            codeGuess[i] = encoderPos;                        //Se o botão for pressionado, aceita o presente número na senha a ser acertada
            pressed = true;
          }
          else
          {
            if (pressed == true)                              //Caso a variável de botão pressionado for verdadeira
            {
              updateDisplayCode();                            //Atualiza o código de exibição para o código atual
              confirmed = true;
            }
          }
        }
      }
      if (encoderPos != prevEncoderPos)                       //Atualizará o código exibido se a posição do encoder mudar
      {
        updateDisplayCode();
        prevEncoderPos = encoderPos;
      }
    }
  }
}

void checkCodeGuess()                                         //Função para verificar se o usuário acertou o código
{
  int correctNum = 0;                                         //Variável para o numero de digitos corretos no lugar errado
  int correctPlace = 0;                                       //Variável para o número de dígitos corretos no lugar correto
  int usedDigits[4] = {0, 0, 0, 0};                           //Marca os dígitos que já foram identificados no lugar errado, evita contar dígitos repetidos duas vezes
  for (int i = 0 ; i <= 3 ; i++)                              //Percorre os quatro digitos do código confirmado do usuário.
  {
    for (int j = 0 ; j <= 3 ; j++)                            //Percorre os quatro digitos do código a ser enviado
    {
      if (codeGuess[i] == code[j])                            //Se um número corresponder certo
      {
        if (usedDigits[j] != 1)                               //Verifica se não foi identificado anteriormente
        {
          correctNum++;                                       //Incrementa na variável de números corretos, não necessariamente no lugar correto
          usedDigits[j] = 1;                                  //Marca os digitos identificados como corretos
          break;                                              //Irá parar de procurar após o digito ser encontrado
        }
      }
    }
  }
  for (int i = 0 ; i <= 3 ; i++)                              //Rotna para verificar se o digito se encontra também no local correto
  {
    if (codeGuess[i] == code[i])                              //Se um dígito correto no lugar correto for encontrado
      correctPlace++;                                         //Incrementa o contador de lugar correto
  }
  updateLEDs(correctNum, correctPlace);                        //Chama a função para atualizar os LEDs com os numeros de corretos e locais corretos
  if (correctPlace == 4)                                       //Se todos os 4 dígitos estiverem corretos, o código foi decifrado
  {
    display.setTextSize(1);
    display.clearDisplay();                                    //Limpa o display
    display.setCursor(30, 10);                                 //Seta o posição do texto
    display.print(F("Desbloqueado!"));                         //Imprime as informações na tela
    display.display();                                         //Exibe o texto
    correctGuess = true;
  }
  else
    correctGuess = false;
}

void updateLEDs (int corNum, int corPla)                        //Função para atualizar os LEDs para mostrar os acertos
{
  for (int i = 0 ; i <= 3 ; i++)                                //Primeiro desliga todos os LEDs
  {
    digitalWrite(correctNumLEDs[i], LOW);
    digitalWrite(correctPlaceLEDs[i], LOW);
  }
  for (int j = 0 ; j <= corNum - 1 ; j++)                       //Ligue o número de dígitos corretos nos LEDs do lugar errado
  {
    digitalWrite(correctNumLEDs[j], HIGH);
  }
  for (int k = 0 ; k <= corPla - 1 ; k++)                       //Ligue o número de dígitos corretos no local correto dos LEDs
  {
    digitalWrite(correctPlaceLEDs[k], HIGH);
  }
}

void startupAni ()                            //Função para a animação inicial
{
  display.setTextSize(2);                     //Imprime as informações na tela size
  display.setCursor(25, 10);                  //Seta o posição do texto
  display.println(F("Decifre"));              //Imprime as informações na tela
  display.display();                          //Exibe o texto
  delay(500);
  display.clearDisplay();                     //Limpa o display
  display.setCursor(55, 10);
  display.println(F("O"));
  display.display();
  delay(500);
  display.clearDisplay();
  display.setCursor(30, 10);
  display.println(F("Codigo"));
  display.display();
  delay(500);
  display.clearDisplay();
}

void PinA()                               //Rotina para tratar a interrupção causada pelo encoder rotativo PinA.
{
  cli();                                  //Pare as interrupções para que seja lido o valor nos pinos
  reading = PIND & 0xC;                   //Lê todos os oito valores dos pinos e remove todos, exceto os valores de pinA e pinB
  if (reading == B00001100 && aFlag)      //Checa se temos os dois os pinos em HIGH e se estavamos esperando uma borda ascendente deste pino
  {
    if (encoderPos > 0)
      encoderPos --;                      //Diminui a contagem da posição do encoder
    else
      encoderPos = 9;                     //Volta para o 9 antes do 0
    bFlag = 0;                            //Reseta as flags para o próximo turno
    aFlag = 0;
  }
  else if (reading == B00000100)          //Sinaliza que estamos esperando o PinB realizar a transição para a rotação
    bFlag = 1;
  sei();                                  //Reinicia as interrupções
}

void PinB()                               //Rotina para tratar a interrupção causada pelo encoder rotativo PinB.
{
  cli();                                  //Pare as interrupções para que seja lido o valor nos pinos
  reading = PIND & 0xC;                   //Lê todos os oito valores dos pinos e remove todos, exceto os valores de pinA e pinB
  if (reading == B00001100 && bFlag)      //Checa se temos os dois os pinos em HIGH e se estavamos esperando uma borda ascendente deste pino
  {
    if (encoderPos < 9)
      encoderPos ++;                      //Incrementa a posição do encoder
    else
      encoderPos = 0;                     //Vá para o 0 depois do 9
    bFlag = 0;                            //Reseta as flags para o próximo turno
    aFlag = 0;
  }
  else if (reading == B00001000)          //Sinaliza que estamos esperando o PinA realizar a transição para a rotação
    aFlag = 1;
  sei();                                   //Reinicia as interrupções
}