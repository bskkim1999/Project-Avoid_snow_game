#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "UART0.h"

/////////////////////////////////////////////////////
//lcd
#define COMMAND_4_BIT_MODE 0x28 //0010 1000
#define RS_PIN 0
#define RW_PIN 1
#define E_PIN 2
#define RS_LOW_EN_high 0x04
#define RS_LOW_EN_low 0x00
#define RS_HIGH_EN_high 0x05
#define RS_HIGH_EN_low 0x01
#define Backlight 0x08
#define COMMAND_DISPLAY_ON_OFF_BIT 2 //0000 0010
#define COMMAND_CLEAR_DISPLAY 0x01 //0000 0001
#define Set_CGRAM_Address 0x40  //0100 0000
#define Set_CGRAM_Address2 0x48 //0100 1000
/////////////////////////////////////////////////////
FILE OUTPUT = FDEV_SETUP_STREAM(UART0_transmit, NULL, _FDEV_SETUP_WRITE);
FILE INPUT = FDEV_SETUP_STREAM(NULL, UART0_receive, _FDEV_SETUP_READ);

volatile unsigned long timer0_millis = 0;
volatile int timer0_micros = 0;

int status[20]={0};
int count_character_moving = 0;
int case_even=0;
int case_odd=0;
int dangerous_flag=0;
int gameover_flag=0;
unsigned char lcd_screen_data_col_0_row_0;
unsigned char lcd_screen_data_col_0_row_1;

unsigned char lcd_screen_data[2][16]={
	//[row][col]
	{' ', ' ', ' ', ' ',' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
	{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}
	
};

unsigned char character_code[8]={0x1F, 0x15, 0x1F, 0x1F, 0x0E, 0x0A, 0x1B, 0x00};
unsigned char gameover_emoji[8]={0x00, 0x11, 0x0A, 0x0A, 0x11, 0x00, 0x0E, 0x11};

ISR(TIMER0_OVF_vect){
	
	unsigned long m = timer0_millis;
	int f = timer0_micros;
	
	m+=1;
	f+=24;
	
	m+=(f/1000);
	f = f % 1000;
	
	timer0_millis = m;
	timer0_micros = f;
	
	
}

//switch character
//택트스위치가 한번 눌려지면 하강엣지에 의해서 캐릭터의 위치가 반전된다. (0행과 1행)
ISR(INT4_vect){
	
	
	//택트 스위치를 한 번 누른 후의 0열 데이터들을 저장한다. (캐릭터의 위치와 눈(*)의 유무를 저장하기 위해서)
	lcd_screen_data_col_0_row_0 = lcd_screen_data[0][0];
	lcd_screen_data_col_0_row_1 = lcd_screen_data[1][0];
	
	
	count_character_moving = (count_character_moving + 1) % 4; //1,2,3,0,1,2,3,....
	//짝수일경우
	if(count_character_moving % 2 == 0) {
		case_even++;
		
	}
	
	//홀수일경우
	else {
		case_odd++;
		
	}
	
	//new code
	if(dangerous_flag==1){
		gameover_flag++; //main 함수의 무한루프 탈출. gameover.
	}
	
}


// TWI(I2C) 통신 초기화 함수
void TWI_Init() {
	// TWI 클럭 주파수 설정 (SCL_freq = CPU_clk / (16 + 2 * TWBR * Prescaler))
	
	DDRD = 0x03; // PD0(SCL), PD1(SDA)
	
	TWSR = (0<<TWPS0) | (0<<TWPS1); //SCL 주파수의 분주비 조절
	TWBR = 72;  // TWBR 값 설정 (주파수를 조절하여 원하는 클럭 주파수 설정)  100khz
	_delay_ms(50);  //setup bus free time
}

// TWI(I2C) 통신 시작 함수
void TWI_Start() {
	TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
	
	while (!(TWCR & (1 << TWINT)));
}

// TWI(I2C) 통신 종료 함수
void TWI_Stop() {
	TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

// TWI(I2C) 통신으로 데이터 전송 함수
void TWI_Write(unsigned char data) {
	
	TWDR = data;
	TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
	
	while (!(TWCR & (1 << TWINT)));
}

/////////////////////////////////////////////////////////
//lcd control source code
void LCD_init(void){
	
	_delay_ms(50);
	
	/*step1 : 3의 값을 3번 줌.*/
	LCD_write_command_4bit(0x30); _delay_ms(5); // 0011 0000
	status[0] = TWSR;
	
	LCD_write_command_4bit(0x30); _delay_us(100); // 0011 0000
	status[1] = TWSR;
	
	LCD_write_command_4bit(0x30); _delay_us(100); // 0011 0000
	status[2] = TWSR;
	
	LCD_write_command_4bit(0x20); _delay_us(100); // 0011 0000
	status[3] = TWSR;
	
	/*step2 : Function set*/ //4비트, 2행, 5*8 폰트
	LCD_write_command_8bit(0x28); _delay_us(50);
	status[4] = TWSR;
	
	/*step3 : display on / off control */
	LCD_write_command_8bit(0x08); _delay_us(50);
	status[5] = TWSR;
	
	/*step4 : clean screen*/
	LCD_clear();  //화면 지움
	status[6] = TWSR;
	
	/*step5 : entry mode set */
	//출력 후 커서를 오른쪽으로 옮김. DDRAM의 주소가 증가하며 화면 이동은 없음.
	LCD_write_command_8bit(0x06);
	status[7] = TWSR;
	/*step6 : display on, cursor & cursor blink off*/
	LCD_write_command_8bit(0x0c);
	status[8] = TWSR;
	
}


void LCD_write_command_4bit(unsigned char command){
	// 상위 4비트만 twi 전송함.
	int lcd_buf[2]={0};
	
	lcd_buf[0] = (command & 0xF0) | RS_LOW_EN_high | Backlight ;  //상위 4비트
	lcd_buf[1] = ( command & 0xF0) | RS_LOW_EN_low | Backlight;
	
	TWI_Write(lcd_buf[0]);  // task1
	again_write_address();
	
	TWI_Write(lcd_buf[1]);  // task2
	again_write_address();
	
	_delay_ms(2);
}

void LCD_write_command_8bit(unsigned char command){
	// 상위 4비트와 하위 4비트를 twi 전송함.
	int lcd_buf[4]={0};
	
	lcd_buf[0] = (command & 0xF0) | RS_LOW_EN_high | Backlight;  //상위 4비트
	lcd_buf[1] = ( command & 0xF0) | RS_LOW_EN_low | Backlight;
	lcd_buf[2] = ( (command << 4)  & 0xF0) | RS_LOW_EN_high | Backlight;  //하위 4비트
	lcd_buf[3] = ( (command << 4) & 0xF0) | RS_LOW_EN_low | Backlight;
	
	TWI_Write(lcd_buf[0]);  // task1
	again_write_address();
	
	TWI_Write(lcd_buf[1]);  // task2
	again_write_address();
	
	TWI_Write(lcd_buf[2]);  // task2
	again_write_address();
	
	TWI_Write(lcd_buf[3]);  // task2
	again_write_address();
	
}

void LCD_clear(void){
	LCD_write_command_8bit(COMMAND_CLEAR_DISPLAY);   //command = 0x01
	_delay_ms(5);
}

void LCD_write_data(unsigned char data){
	
	// 상위 4비트와 하위 4비트를 twi 전송함.
	int lcd_buf[4]={0};
	
	lcd_buf[0] = (data & 0xF0) | RS_HIGH_EN_high | Backlight;  //상위 4비트
	lcd_buf[1] = ( data & 0xF0) | RS_HIGH_EN_low | Backlight;
	lcd_buf[2] = ( (data << 4)  & 0xF0) | RS_HIGH_EN_high | Backlight;  //하위 4비트
	lcd_buf[3] = ( (data << 4) & 0xF0) | RS_HIGH_EN_low | Backlight;
	
	TWI_Write(lcd_buf[0]);  // task1
	again_write_address();
	
	TWI_Write(lcd_buf[1]);  // task2
	again_write_address();
	
	TWI_Write(lcd_buf[2]);  // task2
	again_write_address();
	
	TWI_Write(lcd_buf[3]);  // task2
	again_write_address();
	
	
	
	
}

void again_write_address(void){
	//데이터를 연속적으로 보내기 위해서는 주소를 다시 써야 함.////////
	TWI_Start();
	TWI_Write(0x4E);  //slave address, write mode (0100 1110)
	//_delay_us(100);
	
}

void lcd_goto_XY(unsigned char row, unsigned char col)
{
	unsigned char address = (0x40 * row) + col;
	unsigned char command = 0x80 | address;
	
	LCD_write_command_8bit(command);
}

void LCD_write_string(unsigned char *string){
	unsigned char i;
	for(i=0 ; string[i] != '\0' ; i++){
		LCD_write_data(string[i]);
	}
}


void show_lcd_screen_by_lcd_screen_data(void){
	
	for(int row = 0 ; row < 2 ; row++){
		for(int col = 0; col<16 ; col++){
			lcd_goto_XY(row, col);   //lcd의 지정된 행과 열로 커서를 옮긴 후
			LCD_write_data( lcd_screen_data[row][col]);  // 그 자리에 글자를 write한다.
			
		}
	}
	
}

void write_lcd_screen_data(int row, int col, unsigned char data){
	
	unsigned char (*ptr) [16] = lcd_screen_data;  //2차원 배열의 포인터이다.
	ptr[row][col] = data;
	
	
}

void move_right_to_left_lcd_screen_data(void){
	
	for (int i = 0; i < 2; ++i) {
		for (int j = 1; j < 16 - 1; ++j) {
			lcd_screen_data[i][j] = lcd_screen_data[i][j + 1];
		}
	}
}

void lcd_message_start(void){
	lcd_goto_XY(0,0);
	LCD_write_data('a');
	LCD_write_data('v');
	LCD_write_data('o');
	LCD_write_data('i');
	LCD_write_data('d');
	LCD_write_data(' ');
	LCD_write_data('t');
	LCD_write_data('h');
	LCD_write_data('e');
	LCD_write_data(' ');
	LCD_write_data('s');
	LCD_write_data('n');
	LCD_write_data('o');
	LCD_write_data('w');
	lcd_goto_XY(1,0);
	LCD_write_data('M');
	LCD_write_data('a');
	LCD_write_data('d');
	LCD_write_data('e');
	LCD_write_data(' ');
	LCD_write_data('b');
	LCD_write_data('y');
	LCD_write_data(' ');
	LCD_write_data('b');
	LCD_write_data('o');
	LCD_write_data('s');
	LCD_write_data('e');
	LCD_write_data('o');
	LCD_write_data('k');
}

void lcd_message_end(void){
	
	lcd_goto_XY(0,0);
	LCD_write_data('g');
	LCD_write_data('a');
	LCD_write_data('m');
	LCD_write_data('e');
	LCD_write_data(' ');
	LCD_write_data('o');
	LCD_write_data('v');
	LCD_write_data('e');
	LCD_write_data('r');
	LCD_write_data(' ');
	LCD_write_data(1);
	LCD_write_data(' ');
	LCD_write_data(1);
	LCD_write_data(' ');
	LCD_write_data(1);
	lcd_goto_XY(1,0);
	LCD_write_data('s');
	LCD_write_data('c');
	LCD_write_data('o');
	LCD_write_data('r');
	LCD_write_data('e');
	LCD_write_data(':');
	
	
}




void INT4_external_interrupt(void){
	EIMSK |= (1 << INT4);  //PE4 : external interrupt
	EICRB = 0x02; //0000 0010
}


/////////////////////// timer/counter0  /////////////////////
unsigned long millis(){
	
	unsigned long m;
	//uint8_t oldSREG = SREG;
	
	//cli();
	
	m = timer0_millis;
	
	//SREG = oldSREG;
	
	return m;
	
	
}

void init_timer0(){
	TCCR0 = 1 << CS02;   //pre-scaler : 64
	TIMSK = 1 << TOIE0;  //overflow interrupt accept
	
	sei();
	
	
}
///////////////////////////////////piezo speaker//////////////////////////
void custom_delay(int b){

	for (int x=0 ; x < b ; x ++){
		_delay_ms(1);
	}

}

void speaker_tone(int freq, int duration_ms){
	
	int temp, temp2;
	
	//Top
	temp = (16000000/freq-64)*0.015625;  //소수점은 버려진다. temp가 int형이기 때문이다. (정수형)
	
	//duty-cycle
	temp2 = temp/2;
	OCR1A=temp2; //50%, 10진수 = ?
	
	//Top
	ICR1= temp;
	
	
	custom_delay(duration_ms);
}


void speaker_tone_stop(void){
	
	TCCR1A = 0x00;
	TCCR1B = 0x00;
	speaker_tone(250000, 0);  //speaker_tone() 함수의 temp를 0으로 주기 위해서이다.
	
}

void speaker_tone_init(void){
	DDRB = 0x20; //0010 0000
	
	//모드 : 10 bit fast pwm, non-inverting mode
	TCCR1B |= (1 << WGM13) |(1 << WGM12);
	TCCR1A |= (1 << WGM11) | (0 << WGM10) | (1<<COM1A1) | (0 << COM1A0);
	
	//pre-scaler
	TCCR1B |= (0<<CS12) | (1<<CS11) | (1<<CS10);  //pre-scaler : 64
	
	
}

void speaker_tone_message_start(void){
	speaker_tone(262, 100);  //도
	speaker_tone(294, 100);  //레
	speaker_tone(330, 100);  //미
	speaker_tone(349, 100);  //파
	speaker_tone(392, 100);  //솔
}

void speaker_tone_message_end(void){
	speaker_tone(392, 100);  //솔
	speaker_tone(349, 100);  //파
	speaker_tone(330, 100);  //미
	speaker_tone(294, 100);  //레
	speaker_tone(262, 100);  //도
}

////////////////////////////////////  main  //////////////////////////////////////////


int main() {
	int count_character_moving = 0;
	unsigned long tp_move_screen,tp_snow, tp_speaker_time;
	unsigned long tc_move_screen,tc_snow, tc_speaker_time;
	unsigned long read=0;
	int value = 0;
	int prior_value = 0;
	unsigned char (*ptr2) [16] = lcd_screen_data;  //2차원 배열의 포인터이다.
	int col_1_row_0_flag=0;
	int col_1_row_1_flag=0;
	int score = 0;
	unsigned char arr_score[4];
	int speaker_flag=0;
	float up=0;
	
	stdout = &OUTPUT;
	stdin = &INPUT;
	
	//uart
	UART0_init();
	
	//piezo speaker
	speaker_tone_init();
	speaker_tone_message_start();
	speaker_tone_stop();
	
	DDRA = 0xff;   //output(power 5v, backlight_lcd)
	DDRE = 0x00;  //setup input (PE4 : tact switch)
	
	PORTA = 0x07;  // 0000 0111   (power 5v, backlight_lcd)
	
	//twi communication
	TWI_Init();
	TWI_Start();
	TWI_Write(0x4E);  //address, write mode (0100 1110)
	
	//lcd start
	LCD_init();
	
	//1
	LCD_write_command_8bit(Set_CGRAM_Address);  //사용자 정의 문자를 저장하기 위함.(캐릭터)
	for(int i=0 ; i<8 ; i++){  // 사용자 정의 문자를 저장하는 코드.
		LCD_write_data(character_code[i]);
	}
	
	//2
	LCD_write_command_8bit(Set_CGRAM_Address2);  //사용자 정의 문자를 저장하기 위함. (gameover 이모지)
	for(int i=0 ; i<8 ; i++){  // 사용자 정의 문자를 저장하는 코드.
		LCD_write_data(gameover_emoji[i]);
	}
	
	//Made by boseok 문구를 처음에 lcd screen에 보여준다.
	lcd_message_start();
	_delay_ms(1000);  //1초동안 대기.
	LCD_clear();
	
	
	//interrupt
	sei();  //accept global interrupt
	INT4_external_interrupt();
	write_lcd_screen_data(1,0,0);  //3번째 매개변수의 0은 사용자 정의 캐릭터 코드를 의미한다.
	show_lcd_screen_by_lcd_screen_data();
	
	//timer/counter
	init_timer0();
	
	tp_snow = millis();
	tp_move_screen = millis();
	tp_speaker_time = millis();
	
	while(1){
		//눈(*)을 캐릭터가 위 또는 아래로 맞았을 경우 게임오버. (수시로 검사함.)
		if(gameover_flag==1){
			printf("gameover_!! \r\n");
			break; //무한루프 탈출. gameover.
		}
		
		tc_snow = millis();
		tc_move_screen = millis();
		tc_speaker_time = millis();
		
		
		//task 4 : piezo speaker : 0.1초동안 터치음을 발생함. (터치음이 길게 발생하지 않도록 수시로 검사함.)
		if( (tc_speaker_time - tp_speaker_time >= 100) && (speaker_flag==1) ){
			speaker_flag = 0;
			speaker_tone_stop();
		}
		read = millis(); //seed값
		//task 1 : check every 0.2 seconds, to produce snow
		if(tc_snow - tp_snow > 500 - up){
			if(up <= 400) up = up + 1.1;
			
			tp_snow = tc_snow;
			
			srand(read);  //seed값을 매번 다르게 하기 위하여 타이머/카운터0번의 TCNT0를 사용함.
			value = rand() % 4;  //0 or 1 or 2 or 3
			//0.2초마다 실행.
			//0일경우에는 눈(*)을 1행 15열에 생성.
			printf("value : %d \r\n", value);
			if( (value == 0) && (prior_value != 1)  ){
					
				write_lcd_screen_data(1,15,'*');
				write_lcd_screen_data(0,15,' ');
				//printf("row:1 \r\n");
				prior_value = 0;
			}
			//
			else{
				//1일경우에는 눈(*)을 0행 15열에 생성
				if( (value == 1) && (prior_value != 0)){
						
					write_lcd_screen_data(1,15,' ');
					write_lcd_screen_data(0,15,'*');
					//printf("row:0 \r\n");
					prior_value = 1;
				}
					
				//2,3일 경우에는 아무것도 생성안함. 캐릭터가 눈을 피하기 위한 공간을 형성.
				else{
						
					write_lcd_screen_data(1,15,' ');
					write_lcd_screen_data(0,15,' ');
					//printf("nothing \r\n");
					prior_value = 2;
				}
			}
			show_lcd_screen_by_lcd_screen_data();  //배열의 값을 lcd에 표시해주는 함수
		}
		//task 4 : piezo speaker : 0.1초동안 터치음을 발생함. (터치음이 길게 발생하지 않도록 수시로 검사함.)
		if( (tc_speaker_time - tp_speaker_time >= 100) && (speaker_flag==1) ){
			speaker_flag = 0;
			speaker_tone_stop();
		}
		
		//task 2 : LCD character  (외부인터럽트에 의해 case_even 또는 case_odd의 값이 변경됨)
		if( (case_even == 1) | (case_odd == 1) ){
			
			//piezo speaker(for touch sound)
			tp_speaker_time = tc_speaker_time;
			speaker_flag = 1;
			
			//짝수일경우
			if(case_even == 1){
				write_lcd_screen_data(1,0,0);  //3번째 매개변수의 0은 사용자 정의 캐릭터 코드를 의미한다.
				write_lcd_screen_data(0,0,' ');
			}
			//홀수일경우
			else{
				if(case_odd == 1){
					write_lcd_screen_data(1,0,' ');
					write_lcd_screen_data(0,0,0);  //3번째 매개변수의 0은 사용자 정의 캐릭터 코드를 의미한다.
				}
			}
			show_lcd_screen_by_lcd_screen_data();  //배열의 값을 lcd에 표시해주는 함수
			
			
			//초기화를 한다.
			case_even=0;
			case_odd=0;
			//printf("tc-tp : %lu \r\n", tc_speaker_time - tp_speaker_time);
			
			//piezo speaker
			speaker_tone_init();
			speaker_tone(262, 0);  //도
			
		}
		
		//task 4 : piezo speaker : 0.1초동안 터치음을 발생함. (터치음이 길게 발생하지 않도록 수시로 검사함.)
		if( (tc_speaker_time - tp_speaker_time >= 100) && (speaker_flag==1) ){
			speaker_flag = 0;
			speaker_tone_stop();
		}
		
		//눈(*)을 캐릭터가 위 또는 아래로 맞았을 경우 게임오버. (수시로 검사함.)
		if(gameover_flag==1){
			printf("gameover_!! \r\n");
			break; //무한루프 탈출. gameover.
		}
		
		//task 3 : moving snow.  
		if(tc_move_screen - tp_move_screen  > 500 - up){
			//눈(*)이 움직이는 속도 증가
			if(up <= 400) up = up + 1.1;
			
			//눈(*)을 캐릭터가 위 또는 아래로 맞았을 경우 게임오버. (수시로 검사함.)
			if(gameover_flag==1){
				printf("gameover_!! \r\n");
				break; //무한루프 탈출. gameover.
			}
		
			tp_move_screen = tc_move_screen;   //시간 관련 변수
			
			//lcd_screen_data 의 0열의 값을 저장한다. (캐릭터의 위치와 눈(*)의 유무를 저장하기 위해서)
			lcd_screen_data_col_0_row_0 = lcd_screen_data[0][0];
			lcd_screen_data_col_0_row_1 = lcd_screen_data[1][0];
			
			//check if snow is on col 1    [row][col]
			if( lcd_screen_data[0][1] == '*' ){
				col_1_row_0_flag++;
				
			}
			
			if( lcd_screen_data[1][1] == '*'){
				col_1_row_1_flag++;
				
			}
			
			move_right_to_left_lcd_screen_data();  //배열의 값을 왼쪽으로 한 칸 옮김. (1열 ~ 15열에만 해당함.)
			
			dangerous_flag = 0;  //for case 2
			
			//0열 검사 (만약 0열에 한 곳이라도 눈이 있을 경우에는 move_right_to_left_lcd_screen_data() 호출 이후에는 눈을 제거함.)
			if(lcd_screen_data_col_0_row_0 == '*' ){
				lcd_screen_data[0][0] = ' ';
			}
			
			if(lcd_screen_data_col_0_row_1 == '*'){
				lcd_screen_data[1][0] = ' ';
			}
			
			//case 1 : 눈(*)을 캐릭터가 정면으로 맞았을 경우.
			//만약, 0행 1열에 눈(*)이 존재하고, 0행 0열에 캐릭터가 존재하지 않으면?(캐릭터가 1행 0열에 위치할 경우, 즉,  눈을 피할경우) (참고로 캐릭터는 숫자0이다.)
			if( (col_1_row_0_flag == 1) && (lcd_screen_data_col_0_row_0 != 0)){
				lcd_screen_data[0][0] = '*';  //0행 0열에 눈(*)을 lcd 배열 데이터에 추가한다.
				score++;  //눈을 잘 피했으므로 1점 획득
				//printf("row:0,col:1 \r\n");
			}
			
			
			else{
				//만약, 0행 1열에 눈(*)이 존재하고, 0행 0열에 캐릭터가 존재하여 한 칸 이동 후 겹쳐지면? 게임 오버.
				if( (col_1_row_0_flag == 1) && (lcd_screen_data_col_0_row_0 == 0)){
					printf("test1 \r\n");
					break; //무한루프 탈출!
				}
				
			}
			
			//만약, 1행 1열에 눈(*)이 존재하고, 1행 0열에 캐릭터가 존재하지 않으면? (참고로 캐릭터는 숫자0이다.)
			if( (col_1_row_1_flag == 1) && (lcd_screen_data_col_0_row_1 != 0)){
				lcd_screen_data[1][0] = '*';  //1행 0열에 눈(*)을 lcd 배열 데이터에 추가한다.
				score++;  //눈을 잘 피했으므로 1점 획득
				//printf("row:1,col:1 \r\n");
			}
			else{
				//만약, 1행 1열에 눈(*)이 존재하고, 1행 0열에 캐릭터가 존재하여 한 칸 이동 후 겹쳐지면? 게임 오버.
				if( (col_1_row_1_flag == 1) && (lcd_screen_data_col_0_row_1 == 0)){
					printf("test2 \r\n");
					break; //무한루프 탈출!
				}
				
			}
			//case 2 : 눈(*)을 캐릭터가 위 또는 아래로 맞았을 경우.
			//0행 0열에 눈(*)이 존재하고, 1행 0열에 캐릭터가 존재할 때,  
			if( (lcd_screen_data[0][0] == '*') && (lcd_screen_data[1][0] == 0) ){
				dangerous_flag++;  //dangerous_flag가 1로 바뀜.
			}
			
			//1행 0열에 눈(*)이 존재하고, 0행 0열에 캐릭터가 존재할 때,
			if( (lcd_screen_data[0][0] == 0) && (lcd_screen_data[1][0] == '*') ){
				dangerous_flag++;  //dangerous_flag가 1로 바뀜.
			}
			
			//task 4 : piezo speaker : 0.1초동안 터치음을 발생함. (터치음이 길게 발생하지 않도록 수시로 검사함.)
			if( (tc_speaker_time - tp_speaker_time >= 100) && (speaker_flag==1) ){
				speaker_flag = 0;
				speaker_tone_stop();
			}
			
			//초기화
			col_1_row_0_flag=0;
			col_1_row_1_flag=0;
			
			
			show_lcd_screen_by_lcd_screen_data();  //배열의 값을 lcd에 표시해주는 함수
			
			//task 4 : piezo speaker : 0.1초동안 터치음을 발생함. (터치음이 길게 발생하지 않도록 수시로 검사함.)
			if( (tc_speaker_time - tp_speaker_time >= 100) && (speaker_flag==1) ){
				speaker_flag = 0;
				speaker_tone_stop();
			}
		} //task 3
		
		//눈(*)을 캐릭터가 위 또는 아래로 맞았을 경우 게임오버. (수시로 검사함.)
		if(gameover_flag==1){
			printf("gameover!! \r\n");
			break; //무한루프 탈출. gameover.
		}
		
	} //while(1)
	
	//lcd_write_data() 의 매개변수는 문자만 올 수 있으므로, score(점수)를 문자열로 변경한다.
	sprintf(arr_score, "%d", score);
	
	//게임 실패 이후 lcd 모니터 상황.
	lcd_message_end();
	LCD_write_string(arr_score);
	
	
	//piezo speaker
	speaker_tone_init();
	speaker_tone_message_end();
	speaker_tone_stop();
	
	TWI_Stop();
	
	return 0;
	
}  //main()
