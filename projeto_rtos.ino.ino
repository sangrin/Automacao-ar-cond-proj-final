#include <DHT.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>  
#include <queue.h>  
#include <task.h>

#define DHTPIN 7 // pino DHT
#define DHTTYPE DHT22 // DHT 22

SemaphoreHandle_t xSerialSemaphore; //mutex que vai controlar porta serial

int flag; //controla o buffer para o calculo da media
int k;    /// Contador de preenchimento do buffer da Média.
int i; /// Variável de controle do led.

const int pinLed = 8;

//  vetor que guarda 5 dados lidos do sensor para ser
// calculada a media pela task TempMedia
float bufferTemp[5];

//  Handle da fila que a task DigitalRead envia dados
// lidos do sensor.
QueueHandle_t structQueue;

/// Estrutura utilizada para ler dados do sensor.
struct pinRead {
  int pin; //pino lido da placa
  float value; //valor lido
};

void TaskDigitalRead( void *pvParameters ); //declaracao das tasks
void TaskTempAtual( void *pvParameters );
void TaskTempMedia( void *pvParameters );
void TaskLed( void *pvParameters );

DHT dht(DHTPIN, DHTTYPE); //declaracao do pino e tipo de sensor

void setup() {
  
  Serial.begin(9600); //inicia comunicacao serial
  dht.begin(); //inicia o DHT22

    pinMode(pinLed, OUTPUT); //seta o pinLed como saida

     if ( xSerialSemaphore == NULL ){  //verifica se o semaforo da porta serial ja existe 
    xSerialSemaphore = xSemaphoreCreateMutex();  //cria a mutex que controla a porta serial
    if ( ( xSerialSemaphore ) != NULL )
      xSemaphoreGive( ( xSerialSemaphore ) );  //torna a porta serial disponivel
  }

  
  structQueue = xQueueCreate(5, sizeof(struct pinRead));   //cria a fila de dados

  if (structQueue != NULL) { //verifica se a fila existe                           

  xTaskCreate(TaskDigitalRead,"Digital Read",128,NULL,2,NULL);//cria a tarefa produtora de dados da fila
  
  xTaskCreate(TaskAtual,"Temperatura Atual",128,NULL,2,NULL ); //cria a tarefa para consumir dados da fila
  
  xTaskCreate(TaskMedia,"Temperatura Media",128,NULL,2,NULL ); //cria a tarefa para cálculo da média  
  
  }
//escalonador assume
}

void loop() {
/// Vazio. Tudo é feito nas tasks.
}

///*--------------------------------------------------*/
///*---------------------- Tasks ---------------------*/
///*--------------------------------------------------*/

void TaskDigitalRead( void *pvParameters __attribute__((unused)) )//tarefa que le dados do sensor
{ 
  for (;;){
    struct pinRead currentPinRead;
    currentPinRead.pin = 7;

    currentPinRead.value = dht.readTemperature(); //adquire temperatura 
    
   /// Posta um item na fila.
   /// https://www.freertos.org/a00117.html
    
    xQueueSend(structQueue, &currentPinRead, portMAX_DELAY);
    vTaskDelay(1);  /// Um tick de atraso (15ms) entre as leituras para estabilidade.
  }
}

void TaskAtual( void *pvParameters __attribute__((unused)) ) //tarefa que consome dado do buffer se disponível;
{
  for (;;){
    struct pinRead currentPinRead;

     //le item da fila
     // https://www.freertos.org/a00118.html
    if (xQueueReceive(structQueue, &currentPinRead, portMAX_DELAY) == pdPASS) {
      
      bufferTemp[k]=currentPinRead.value;  
      
      if(k<5){ //verifica se ainda nao foram armazenados 5 dados no buffer da media
       i = k; // a variavel de controle do led recebe contador do buffer 
       flag=0; // Caso não, flag continua em 0.
       
       if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){ // verifica se o semaforo esta disponivel; se sim, continua
        
        Serial.print("Temperatura Atual: "); 
        Serial.println(currentPinRead.value);  //imprime no monitor serial o valor lido
        Serial.println(k); //posicao do buffer no momento
        xSemaphoreGive( xSerialSemaphore ); //libera a porta serial
        k++;   //incrementa a variavel de controle do buffer
       }
      } else{   //caso o contador atinja 5 (fila cheia)
        i=0;    /// reseta a variavel para evitar leitura do buffer
        flag=1; // flag = 1 e a media pode ser calculada
        }   
    }
  }   
}

void TaskMedia( void *pvParameters __attribute__((unused)) ) //tarefa que consome o buffer para calculo
{
  for (;;){
    float media; //variavel que guarda a media
    float acumulado; //guarda o total de dados do buffer
   
    if(flag==1){ //verifica se a flag foi alterada
     
      for (int j=0;j<5;j++){ 
        acumulado = acumulado + bufferTemp[j]; 
      } 
      media=acumulado/5; //calcula media

         if(media>=24){ //se a temperatura media for maior que 24, liga o led
      digitalWrite(pinLed,HIGH);
         }else if(media<=19){
        digitalWrite(pinLed,LOW);
         }
      flag=0; //dados do buffer consumidos, flag resetada
      k=0;    //reseta variavel de controle do buffer
     
      if ( xSemaphoreTake( xSerialSemaphore, ( TickType_t ) 5 ) == pdTRUE ){   //verifica se o semaforo esta disponivel
        
        Serial.print("Media: "); //imprime a media no monitor serial
        Serial.println(media);
        
        media=0; //reseta media
        acumulado=0;//reseta total
        
        xSemaphoreGive( xSerialSemaphore ); //libera serial
      }     
    }
    else{flag=0;i=k;} //nao calcula media se nao foram lidos 5 valores
  } 


}   


