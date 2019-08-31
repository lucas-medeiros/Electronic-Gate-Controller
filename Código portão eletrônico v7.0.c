//*********************************************************************
//   PROJETO: Portão eletrônico
//   
//   Controle de Versão: 7.0
//   Autor: Lucas e Jefferson

//  Num |       Evento     			 |   Data     | Observações
//  1°  | Criação do código			 | 	 	  	  | 
//  2º  | Falta resolver uns BO		 | 21/09/2018 |
//  3º  | Lógica Nova		   		 | 26/09/2018 |
//  4º  | Lógica nova da nova  		 | 27/06/2018 |
//  5º  | Resolução de bugs    		 | 28/09/2018 |
//  6º  | Bug do FCF resolvido 		 | 29/09/2018 |
//  7º  | Resolução de bugs do timer | 04/10/2018 |


//*******************************************************************

//************************************** ÁREA DE INCLUSÃO DE BIBLIOTECAS ***************************************

#include <p18f4550.h> // biblioteca do microcontrolador 
#include <delays.h> //Adiciona a biblioteca de delay
#include <pwm.h>    //Adiciona a biblioteca do pwm              
#include <timers.h> //Adiciona a biblioteca do timer 
      
//************************************** ÁREA DE DEFINES - APELIDOS ****************************************
//entradas:
#define FCA PORTBbits.RB0  //fim de curso aberto
#define FCF PORTBbits.RB1  //fim de curso fechado
#define Botao_PARA PORTBbits.RB2  //botão de Parada
#define Botao_ABRE PORTBbits.RB3  //botão de Abrir
#define Botao_FECHA PORTBbits.RB4 //botão de Fechar

//saidas:
#define led_emergencia PORTDbits.RD0 //led de emergencia adicional (extra)


//************************************** ÁREA DE AJUSTE DOS BITS DE CONFIGURAÇÃO *************************************

#pragma config FOSC = INTOSC_EC // Habilita o oscilador interno
#pragma config WDT = OFF    //Desabilita o Watchdog Timer (WDT).
#pragma config PWRT = ON   //Habilita o Power-up Timer (PWRT).
#pragma config BOR = OFF   //Brown-out Reset (BOR) desabilitado.
#pragma config PBADEN = OFF   //RB0,1,2,3 e 4 configurado como I/O digital.
#pragma config LVP = OFF   //Desabilita o Low Voltage Program.           

//************************************** ÁREA DE PROTÓTIPO DE FUNÇÕES **************************************
void VerificaEstado(void);
void configuraInt(void);
void FechaPortao(void);
void AbrePortao(void);
void FreiaPortao(void);
void SetLedEmergencia(void);
void DesligaLedEmergencia(void);
void ZeraTimer(void);
void ISR_tratamento(void);
void interrupcao_baixa_prior(void);
void INT_Timer_tratamento(void);
void configuraTimer0(void);
void CarregaRegistradorTimer(void);
void HabilitaTimer0(void);
void DesabilitaTimer0(void);

//************************************** ÁREA DE VARIAVEIS GLOBAIS  ****************************************

unsigned char period = 255; //período do pwm
unsigned int duty_cycle_rapido = 632; //variável usada para usar o pwm rápido (abre portão)
unsigned int duty_cycle_lento = 250; //variável usada para usar o pwm lento (fecha portão)
unsigned int freio = 0; //variável usada para parar o pwm

unsigned int timer = 0; //variável de controle do timer

unsigned int estado = 0; //variável de controle do estado do portão
//1 = aberto
//2 = fechado
//3 = semi aberto
//4 = deu BO

//************ Definição da função que será chamada quando ocorrer a interrupção **************************************

//interrupção de alta prioridade:
#pragma code interrupcao_alta_prior = 0x08 // vetor de interrupção de alta prioridade

void interrupcao_alta_prior(void)
{
   _asm 
      goto ISR_tratamento 
   _endasm
}
#pragma code

//baixa prioridade
#pragma code interrupcao_baixa_prior = 0x18 // vetor de interrupção de baixa prioridade

void interrupcao_baixa_prior(void){
	_asm
		goto INT_Timer_tratamento
	_endasm
}
#pragma code
//************************************** ÁREA DE TRATAMENTO DE INTERRUPÇÕES***************************************

#pragma interrupt ISR_tratamento
void ISR_tratamento(void) //botaoPara
{
	// codigo de tratamento
	FreiaPortao();
	while(1){ //deixa o motor parado até algum botão ser pressionado
		if ((Botao_ABRE == 1) || (Botao_FECHA == 1)){
			break;
		}
	}
  
	INTCON3bits.INT2IF = 0; //limpa o flag de interrupção  
}

#pragma interrupt INT_Timer_tratamento
void INT_Timer_tratamento(void){ //interrupt do timer 0
	//código de tratamento
	timer = 1;
	CarregaRegistradorTimer();//65535 - 39062 = 26473; carrega o valor no registrador toda vez que a interrupção estoura
	INTCONbits.TMR0IF = 0;// limpa flag de interrupção do timer 0 
	
	FechaPortao();
}

//************************************** ÁREA DO PROGRAMA PRINCIPAL ****************************************
void main(void)
{ 
	
	TRISA = 0b00000000; //RA – como saida 
	TRISB = 0b11111111; //RB0 a RB7 – como entrada
	TRISD = 0b00000000; //RD0 a RD7 - como saida
	TRISC = 0b10000000; //pro modulo PWM
	ADCON1 = 0X0f;

	OSCCON = 0xF2; // configura oscilador interno para 8 MHz
   
	configuraInt();
	configuraTimer0();
   
	DesligaLedEmergencia(); //led_emergencia = 0;
  
	OpenTimer2(TIMER_INT_OFF & T2_PS_1_1 & T2_POST_1_1); //timer do PWM

	OpenPWM1 (period);              
	OpenPWM2 (period); 
	
	//precisa ficar fora do loop para que caso haja ruido no sinal o portão não abra
	if ((FCA == 0) && (FCF == 0) && (Botao_ABRE == 0) && (Botao_FECHA == 0)){ //portão semi aberto e botões em 0
		AbrePortao();
	}
	
	while(1){ //loop infinito
		
		if ( ((FCA == 1) && (FCF == 1)) || ((Botao_ABRE == 1) && (Botao_FECHA == 1)) ){
			//caso exepcional: dois botões pressionados ao mesmo tempo
			//					ou dois fins de curso ao mesmo tempo (portão aberto e fechado ao mesmo tempo?) -> deu BO
			SetLedEmergencia();
		}else{
			
			DesligaLedEmergencia();
			if (Botao_PARA == 1){ //garante que o motor continua parado caso o botão de parada seja pressionado
				FreiaPortao();
			}else{
				
				if (FCF == 1){ //garante que o motor continua parado caso esteja fechado
					FreiaPortao();
				}else{
					
					if ((FCA == 0) && (FCF == 0) && (Botao_ABRE == 0) && (Botao_FECHA == 1)){ //portão semi aberto e botão de fecha em 1
						FechaPortao();
					}
					if ((FCA == 0) && (FCF == 0) && (Botao_ABRE == 1) && (Botao_FECHA == 0)){ //portão semi aberto e botão de abre em 1
						AbrePortao();
					}
					if ((FCA == 0) && (FCF == 1) && (Botao_ABRE == 0)){ //portão fechado
						FreiaPortao();
					}
					if ((FCA == 0) && (FCF == 1) && (Botao_ABRE == 1)){ //portão fechado e botão de abre em 1
						//AbrePortao();
						FreiaPortao();
					}
					if ((FCA == 1) && (FCF == 0) && (Botao_FECHA == 1)){ //portão aberto e botão de fecha em 1
						FechaPortao();
						CarregaRegistradorTimer();
					}
					if ((FCA == 1) && (FCF == 0) && (Botao_FECHA == 0)){ //portão aberto
						
						FreiaPortao();
						
						//set timer pra contar 5 segundos
						HabilitaTimer0();  //habilita interrup do timer 0 -> INTCONbits.TMR0IE = 1;
						//CarregaRegistradorTimer();
						//quando estoura interrupção timer = 1;
						
						while (timer == 0){ //loop enquanto interrupção não estoura e botão não é pressionado
							if ((FCF == 1) && (FCA == 1)){
								SetLedEmergencia();
								FreiaPortao();
							}
								
							if (Botao_FECHA == 1){
								timer = 1;
								DesabilitaTimer0(); //desabilita interrup do timer 0 -> INTCONbits.TMR0IE = 0; 
								FechaPortao();
								
								//CarregaRegistradorTimer();
								
								break;
							}
						}
						timer = 0;
						DesabilitaTimer0(); //desabilita interrup do timer 0 -> INTCONbits.TMR0IE = 0; 
						DesligaLedEmergencia();
						CarregaRegistradorTimer();
						
						FreiaPortao();
						
					}
				}
			}
			DesligaLedEmergencia();
		}
		
	}
	
    
}

// ************************************** ÁREA DAS FUNÇÕES *****************************************

void VerificaEstado(){
	
	if ((FCA == 1) && (FCF == 1)){ // deu BO
		estado = 4;
		
	}
	if ((Botao_ABRE == 1) && (Botao_FECHA == 1)){ //deu BO
		estado = 4;
		
	}
	if ((FCA == 1) && (FCF == 0)){ // ta aberto - deve contar o tempo pra fechar
		estado = 1;
		
	}
	if ((FCA == 0) && (FCF == 1)){ // ta fechado - deve seguir o funcionamento normal
		estado = 2;
		
	}else{ // ta semi aberto - deve abrir e contar o tempo pra fechar [ if(FCA ==0) && (FCF ==0) ]
		estado = 3;
	}
}

void configuraInt(){// função de configuração da interrupção
	//------------------------------------------------------------------------------
	// configuração da interrupção
	//------------------------------------------------------------------------------
	INTCON2bits.INTEDG2 = 1; // seleção de borda de subida em RB2: INT ext 2
	INTCON3bits.INT2IF = 0; // limpa o flag de interrupção externa 2
	INTCON3bits.INT2IP = 1; // seleção de alta prioridade
	INTCON3bits.INT2IE = 1; // ativação da interrupção externa INT 2 (RB2)
	RCONbits.IPEN = 1; //Habilita a interrupção com nível prioridade alta.
	// Endereço do vetor: 0x08
	INTCONbits.GIEH = 1; // Habilita todas as interrupções de alta prioridade
	INTCONbits.GIEL = 1; // Habilita todas as interrupções de baixa prioridade
	//------------------------------------------------------------------------------
}

void configuraTimer0(){
	//------------------------------------------------------------------------------
	// configuração da interrupção timer 0
	//------------------------------------------------------------------------------
	timer = 0;
	
	OpenTimer0 (TIMER_INT_ON & T0_16BIT &T0_SOURCE_INT & T0_PS_1_256); // configura o Timer 0 com interrupção ligada
	
	//timer 0 -> interrupção inicialmente ligada | modo: 16 bits | fonte: oscilador interno | pós scaler: 256

	
	INTCONbits.TMR0IE = 0; //desabilita interrupção pro timer 0
	INTCONbits.TMR0IF = 0; //limpa flag de interrupção
	INTCON2bits.TMR0IP = 0; //seleciona baixa prioridade
	// Endereço do vetor: 0x18
	
	//0x0FFFF = 65535d
	//(((8MHz / 4) / (0,2 * 256) ) = 39062
	//65535 - 39062 = 26473
	
	CarregaRegistradorTimer();
	
	//ja habilitadas:
	//INTCONbits.GIEL = 1;// habilita a chave geral de interrupções
	//INTCONbits.GIE = 1;// habilita a chave geral de interrupções de periféricos
}

void FechaPortao(){
	DesabilitaTimer0();
	while((FCF != 1) && (Botao_ABRE != 1)){ //quando fim de curso pressionado -> portão terminou de fechar
		//PWM lento
		SetDCPWM1(duty_cycle_lento);
		SetDCPWM2(0);
	}
	estado = 2; //portão fechado
	FreiaPortao(); //para o motor
	CarregaRegistradorTimer();
}

void AbrePortao(){
	DesabilitaTimer0();
	while((FCA != 1)&&(Botao_FECHA != 1) ){ //quando fim de curso pressionado -> portão terminou de abrir
		//PWM rapido
		SetDCPWM1(0);
		SetDCPWM2(duty_cycle_rapido);
	}
	estado = 1; //portão aberto
	FreiaPortao(); //para o motor
	CarregaRegistradorTimer();
}

void FreiaPortao(){
	//coloca PWM em 50/50
	SetDCPWM1(freio); //freio = 0
	SetDCPWM2(freio);
}

void SetLedEmergencia(){
	led_emergencia = 1;
}

void DesligaLedEmergencia(){
	led_emergencia = 0;
}

void CarregaRegistradorTimer(){
	TMR0L = 0x69;
	TMR0H = 0x67;
	//WriteTimer0(26473);
}

void HabilitaTimer0(){
	CarregaRegistradorTimer();
	INTCONbits.TMR0IF = 0;// limpa flag de interrupção do timer 0 	
	T0CONbits.TMR0ON = 1; 
	INTCONbits.TMR0IE = 1;
}

void DesabilitaTimer0(){
	T0CONbits.TMR0ON = 0;
	INTCONbits.TMR0IE = 0;
}

/************************************** FIM DO CÓDIGO ***************************************/