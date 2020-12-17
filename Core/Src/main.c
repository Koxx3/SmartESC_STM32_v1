/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdlib.h> // for abs()
#include "stm32f1xx_hal.h"
#include "defines.h"
#include "config.h"
#include "util.h"
#include "comms.h"
#include "BLDC_controller.h"      /* BLDC's header file */
#include "rtwtypes.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_ADC2_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void SystemClock_Config(void);

//------------------------------------------------------------------------
// Global variables set externally
//------------------------------------------------------------------------
extern TIM_HandleTypeDef htim_left;
extern TIM_HandleTypeDef htim_right;
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern volatile adc_buf_t adc_buffer;
#if defined(DEBUG_I2C_LCD) || defined(SUPPORT_LCD)
  extern LCD_PCF8574_HandleTypeDef lcd;
  extern uint8_t LCDerrorFlag;
#endif

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

// Matlab defines - from auto-code generation
//---------------
extern P    rtP_Left;                   /* Block parameters (auto storage) */
extern P    rtP_Right;                  /* Block parameters (auto storage) */
extern ExtY rtY_Left;                   /* External outputs */
extern ExtY rtY_Right;                  /* External outputs */
//---------------

extern int16_t cmd1;                    // normalized input value. -1000 to 1000
extern int16_t cmd2;                    // normalized input value. -1000 to 1000
extern int16_t input1;                  // Non normalized input value
extern int16_t input2;                  // Non normalized input value

extern int16_t speedAvg;                // Average measured speed
extern int16_t speedAvgAbs;             // Average measured speed in absolute
extern volatile uint32_t timeoutCnt;    // Timeout counter for the General timeout (PPM, PWM, Nunchuck)
extern uint8_t timeoutFlagADC;          // Timeout Flag for for ADC Protection: 0 = OK, 1 = Problem detected (line disconnected or wrong ADC data)
extern uint8_t timeoutFlagSerial;       // Timeout Flag for Rx Serial command: 0 = OK, 1 = Problem detected (line disconnected or wrong Rx data)

extern volatile int pwml;               // global variable for pwm left. -1000 to 1000
extern volatile int pwmr;               // global variable for pwm right. -1000 to 1000

extern uint8_t enable;                  // global variable for motor enable

extern int16_t batVoltage;              // global variable for battery voltage

#if defined(SIDEBOARD_SERIAL_USART2)
extern SerialSideboard Sideboard_L;
#endif
#if defined(SIDEBOARD_SERIAL_USART3)
extern SerialSideboard Sideboard_R;
#endif
#if (defined(CONTROL_PPM_LEFT) && defined(DEBUG_SERIAL_USART3)) || (defined(CONTROL_PPM_RIGHT) && defined(DEBUG_SERIAL_USART2))
extern volatile uint16_t ppm_captured_value[PPM_NUM_CHANNELS+1];
#endif
#if (defined(CONTROL_PWM_LEFT) && defined(DEBUG_SERIAL_USART3)) || (defined(CONTROL_PWM_RIGHT) && defined(DEBUG_SERIAL_USART2))
extern volatile uint16_t pwm_captured_ch1_value;
extern volatile uint16_t pwm_captured_ch2_value;
#endif


//------------------------------------------------------------------------
// Global variables set here in main.c
//------------------------------------------------------------------------
uint8_t backwardDrive;
volatile uint32_t main_loop_counter;

//------------------------------------------------------------------------
// Local variables
//------------------------------------------------------------------------
#if defined(FEEDBACK_SERIAL_USART2) || defined(FEEDBACK_SERIAL_USART3)
typedef struct{
  uint16_t  start;
  int16_t   cmd1;
  int16_t   cmd2;
  int16_t   speedR_meas;
  int16_t   speedL_meas;
  int16_t   batVoltage;
  int16_t   boardTemp;
  uint16_t  cmdLed;
  uint16_t  checksum;
} SerialFeedback;
static SerialFeedback Feedback;
#endif
#if defined(FEEDBACK_SERIAL_USART2)
static uint8_t sideboard_leds_L;
#endif
#if defined(FEEDBACK_SERIAL_USART3)
static uint8_t sideboard_leds_R;
#endif

#ifdef VARIANT_TRANSPOTTER
  extern uint8_t  nunchuk_connected;
  extern float    setDistance;

  static uint8_t  checkRemote = 0;
  static uint16_t distance;
  static float    steering;
  static int      distanceErr;
  static int      lastDistance = 0;
  static uint16_t transpotter_counter = 0;
#endif

static int16_t    speed;                // local variable for speed. -1000 to 1000
#ifndef VARIANT_TRANSPOTTER
  static int16_t  steer;                // local variable for steering. -1000 to 1000
  static int16_t  steerRateFixdt;       // local fixed-point variable for steering rate limiter
  static int16_t  speedRateFixdt;       // local fixed-point variable for speed rate limiter
  static int32_t  steerFixdt;           // local fixed-point variable for steering low-pass filter
  static int32_t  speedFixdt;           // local fixed-point variable for speed low-pass filter
#endif

static uint32_t    inactivity_timeout_counter;
static MultipleTap MultipleTapBrake;    // define multiple tap functionality for the Brake pedal

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  __HAL_RCC_AFIO_CLK_ENABLE();
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
  /* System interrupt init*/
  /* MemoryManagement_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
  /* BusFault_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);
  /* UsageFault_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0);
  /* SVCall_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SVCall_IRQn, 0, 0);
  /* DebugMonitor_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DebugMonitor_IRQn, 0, 0);
  /* PendSV_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(PendSV_IRQn, 0, 0);
  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  __HAL_RCC_DMA1_CLK_DISABLE();

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_TIM1_Init();
  MX_USART3_UART_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  BLDC_Init();        // BLDC Controller Init

#if KX
  HAL_GPIO_WritePin(OFF_PORT, OFF_PIN, GPIO_PIN_SET);   // Activate Latch
#endif

  Input_Lim_Init();   // Input Limitations Init
  Input_Init();       // Input Init

  HAL_ADC_Start(&hadc1);
  HAL_ADC_Start(&hadc2);

#if KX
  poweronMelody();
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
#endif

  int16_t speedL     = 0, speedR     = 0;
  int16_t lastSpeedL = 0, lastSpeedR = 0;

  int32_t board_temp_adcFixdt = adc_buffer.temp << 16;  // Fixed-point filter output initialized with current ADC converted to fixed-point
  int16_t board_temp_adcFilt  = adc_buffer.temp;
  int16_t board_temp_deg_c;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	    HAL_Delay(DELAY_IN_MAIN_LOOP);        //delay in ms

	    readCommand();                        // Read Command: cmd1, cmd2
	    calcAvgSpeed();                       // Calculate average measured speed: speedAvg, speedAvgAbs

	    #ifndef VARIANT_TRANSPOTTER
	      // ####### MOTOR ENABLING: Only if the initial input is very small (for SAFETY) #######
	      if (enable == 0 && (!rtY_Left.z_errCode && !rtY_Right.z_errCode) && (cmd1 > -50 && cmd1 < 50) && (cmd2 > -50 && cmd2 < 50)){
	        beepShort(6);                     // make 2 beeps indicating the motor enable
	        beepShort(4); HAL_Delay(100);
	        steerFixdt = speedFixdt = 0;      // reset filters
	        enable = 1;                       // enable motors
	        consoleLog("-- Motors enabled --\r\n");
	      }

	      // ####### VARIANT_HOVERCAR #######
	      #if defined(VARIANT_HOVERCAR) || defined(VARIANT_SKATEBOARD) || defined(ELECTRIC_BRAKE_ENABLE)
	        uint16_t speedBlend;                                        // Calculate speed Blend, a number between [0, 1] in fixdt(0,16,15)
	        speedBlend = (uint16_t)(((CLAMP(speedAvgAbs,10,60) - 10) << 15) / 50); // speedBlend [0,1] is within [10 rpm, 60rpm]
	      #endif

	      #ifdef STANDSTILL_HOLD_ENABLE
	        standstillHold();                                           // Apply Standstill Hold functionality. Only available and makes sense for VOLTAGE or TORQUE Mode
	      #endif

	      #ifdef VARIANT_HOVERCAR
	        if (speedAvgAbs < 60) {                                     // Check if Hovercar is physically close to standstill to enable Double tap detection on Brake pedal for Reverse functionality
	          multipleTapDet(cmd1, HAL_GetTick(), &MultipleTapBrake);   // Brake pedal in this case is "cmd1" variable
	        }

	        if (cmd1 > 30) {                                            // If Brake pedal (cmd1) is pressed, bring to 0 also the Throttle pedal (cmd2) to avoid "Double pedal" driving
	          cmd2 = (int16_t)((cmd2 * speedBlend) >> 15);
	          cruiseControl((uint8_t)rtP_Left.b_cruiseCtrlEna);         // Cruise control deactivated by Brake pedal if it was active
	        }
	      #endif

	      #ifdef ELECTRIC_BRAKE_ENABLE
	        electricBrake(speedBlend, MultipleTapBrake.b_multipleTap);  // Apply Electric Brake. Only available and makes sense for TORQUE Mode
	      #endif

	      #ifdef VARIANT_HOVERCAR
	        if (speedAvg > 0) {                                         // Make sure the Brake pedal is opposite to the direction of motion AND it goes to 0 as we reach standstill (to avoid Reverse driving by Brake pedal)
	          cmd1 = (int16_t)((-cmd1 * speedBlend) >> 15);
	        } else {
	          cmd1 = (int16_t)(( cmd1 * speedBlend) >> 15);
	        }
	      #endif

	      #ifdef VARIANT_SKATEBOARD
	        if (cmd2 < 0) {                                           // When Throttle is negative, it acts as brake. This condition is to make sure it goes to 0 as we reach standstill (to avoid Reverse driving)
	          if (speedAvg > 0) {                                     // Make sure the braking is opposite to the direction of motion
	            cmd2 = (int16_t)(( cmd2 * speedBlend) >> 15);
	          } else {
	            cmd2 = (int16_t)((-cmd2 * speedBlend) >> 15);
	          }
	        }
	      #endif

	      // ####### LOW-PASS FILTER #######
	      rateLimiter16(cmd1, RATE, &steerRateFixdt);
	      rateLimiter16(cmd2, RATE, &speedRateFixdt);
	      filtLowPass32(steerRateFixdt >> 4, FILTER, &steerFixdt);
	      filtLowPass32(speedRateFixdt >> 4, FILTER, &speedFixdt);
	      steer = (int16_t)(steerFixdt >> 16);  // convert fixed-point to integer
	      speed = (int16_t)(speedFixdt >> 16);  // convert fixed-point to integer

	      // ####### VARIANT_HOVERCAR #######
	      #ifdef VARIANT_HOVERCAR
	        if (!MultipleTapBrake.b_multipleTap) {  // Check driving direction
	          speed = steer + speed;                // Forward driving: in this case steer = Brake, speed = Throttle
	        } else {
	          speed = steer - speed;                // Reverse driving: in this case steer = Brake, speed = Throttle
	        }
	      #endif

	      // ####### MIXER #######
	      // speedR = CLAMP((int)(speed * SPEED_COEFFICIENT -  steer * STEER_COEFFICIENT), INPUT_MIN, INPUT_MA);
	      // speedL = CLAMP((int)(speed * SPEED_COEFFICIENT +  steer * STEER_COEFFICIENT), INPUT_MIN, INPUT_MA);
	      mixerFcn(speed << 4, steer << 4, &speedR, &speedL);   // This function implements the equations above

	      // ####### SET OUTPUTS (if the target change is less than +/- 100) #######
	      if ((speedL > lastSpeedL-100 && speedL < lastSpeedL+100) && (speedR > lastSpeedR-100 && speedR < lastSpeedR+100)) {
	        #ifdef INVERT_R_DIRECTION
	          pwmr = speedR;
	        #else
	          pwmr = -speedR;
	        #endif
	        #ifdef INVERT_L_DIRECTION
	          pwml = -speedL;
	        #else
	          pwml = speedL;
	        #endif
	      }
	    #endif

	    #ifdef VARIANT_TRANSPOTTER
	      distance    = CLAMP(cmd1 - 180, 0, 4095);
	      steering    = (cmd2 - 2048) / 2048.0;
	      distanceErr = distance - (int)(setDistance * 1345);

	      if (nunchuk_connected == 0) {
	        speedL = speedL * 0.8f + (CLAMP(distanceErr + (steering*((float)MAX(ABS(distanceErr), 50)) * ROT_P), -850, 850) * -0.2f);
	        speedR = speedR * 0.8f + (CLAMP(distanceErr - (steering*((float)MAX(ABS(distanceErr), 50)) * ROT_P), -850, 850) * -0.2f);
	        if ((speedL < lastSpeedL + 50 && speedL > lastSpeedL - 50) && (speedR < lastSpeedR + 50 && speedR > lastSpeedR - 50)) {
	          if (distanceErr > 0) {
	            enable = 1;
	          }
	          if (distanceErr > -300) {
	            #ifdef INVERT_R_DIRECTION
	              pwmr = speedR;
	            #else
	              pwmr = -speedR;
	            #endif
	            #ifdef INVERT_L_DIRECTION
	              pwml = -speedL;
	            #else
	              pwml = speedL;
	            #endif

	            if (checkRemote) {
	              if (!HAL_GPIO_ReadPin(LED_PORT, LED_PIN)) {
	                //enable = 1;
	              } else {
	                enable = 0;
	              }
	            }
	          } else {
	            enable = 0;
	          }
	        }
	        timeoutCnt = 0;
	      }

	      if (timeoutCnt > TIMEOUT) {
	        pwml = 0;
	        pwmr = 0;
	        enable = 0;
	        #ifdef SUPPORT_LCD
	          LCD_SetLocation(&lcd,  0, 0); LCD_WriteString(&lcd, "Len:");
	          LCD_SetLocation(&lcd,  8, 0); LCD_WriteString(&lcd, "m(");
	          LCD_SetLocation(&lcd, 14, 0); LCD_WriteString(&lcd, "m)");
	        #endif
	        HAL_Delay(1000);
	        nunchuk_connected = 0;
	      }

	      if ((distance / 1345.0) - setDistance > 0.5 && (lastDistance / 1345.0) - setDistance > 0.5) { // Error, robot too far away!
	        enable = 0;
	        beepLong(5);
	        #ifdef SUPPORT_LCD
	          LCD_ClearDisplay(&lcd);
	          HAL_Delay(5);
	          LCD_SetLocation(&lcd, 0, 0); LCD_WriteString(&lcd, "Emergency Off!");
	          LCD_SetLocation(&lcd, 0, 1); LCD_WriteString(&lcd, "Keeper too fast.");
	        #endif
	        poweroff();
	      }

	      #ifdef SUPPORT_NUNCHUK
	        if (transpotter_counter % 500 == 0) {
	          if (nunchuk_connected == 0 && enable == 0) {
	            if (Nunchuk_Ping()) {
	              HAL_Delay(500);
	              Nunchuk_Init();
	              #ifdef SUPPORT_LCD
	                LCD_SetLocation(&lcd, 0, 0); LCD_WriteString(&lcd, "Nunchuk Control");
	              #endif
	              timeoutCnt = 0;
	              HAL_Delay(1000);
	              nunchuk_connected = 1;
	            }
	          }
	        }
	      #endif

	      #ifdef SUPPORT_LCD
	        if (transpotter_counter % 100 == 0) {
	          if (LCDerrorFlag == 1 && enable == 0) {

	          } else {
	            if (nunchuk_connected == 0) {
	              LCD_SetLocation(&lcd,  4, 0); LCD_WriteFloat(&lcd,distance/1345.0,2);
	              LCD_SetLocation(&lcd, 10, 0); LCD_WriteFloat(&lcd,setDistance,2);
	            }
	            LCD_SetLocation(&lcd,  4, 1); LCD_WriteFloat(&lcd,batVoltage, 1);
	            // LCD_SetLocation(&lcd, 11, 1); LCD_WriteFloat(&lcd,MAX(ABS(currentR), ABS(currentL)),2);
	          }
	        }
	      #endif
	      transpotter_counter++;
	    #endif

	    // ####### SIDEBOARDS HANDLING #######
	    #if defined(SIDEBOARD_SERIAL_USART2)
	      sideboardLeds(&sideboard_leds_L);
	      sideboardSensors((uint8_t)Sideboard_L.sensors);
	    #endif
	    #if defined(SIDEBOARD_SERIAL_USART3)
	      sideboardLeds(&sideboard_leds_R);
	      sideboardSensors((uint8_t)Sideboard_R.sensors);
	    #endif

	    // ####### CALC BOARD TEMPERATURE #######
	    filtLowPass32(adc_buffer.temp, TEMP_FILT_COEF, &board_temp_adcFixdt);
	    board_temp_adcFilt  = (int16_t)(board_temp_adcFixdt >> 16);  // convert fixed-point to integer
	    board_temp_deg_c    = (TEMP_CAL_HIGH_DEG_C - TEMP_CAL_LOW_DEG_C) * (board_temp_adcFilt - TEMP_CAL_LOW_ADC) / (TEMP_CAL_HIGH_ADC - TEMP_CAL_LOW_ADC) + TEMP_CAL_LOW_DEG_C;

	    // ####### DEBUG SERIAL OUT #######
	    #if defined(DEBUG_SERIAL_USART2) || defined(DEBUG_SERIAL_USART3)
	      if (main_loop_counter % 25 == 0) {    // Send data periodically every 125 ms
	        setScopeChannel(0, (int16_t)input1);                    // 1: INPUT1
	        setScopeChannel(1, (int16_t)input2);                    // 2: INPUT2
	        setScopeChannel(2, (int16_t)speedR);                    // 3: output command: [-1000, 1000]
	        setScopeChannel(3, (int16_t)speedL);                    // 4: output command: [-1000, 1000]
	        setScopeChannel(4, (int16_t)adc_buffer.batt1);          // 5: for battery voltage calibration
	        setScopeChannel(5, (int16_t)(batVoltage * BAT_CALIB_REAL_VOLTAGE / BAT_CALIB_ADC)); // 6: for verifying battery voltage calibration
	        setScopeChannel(6, (int16_t)board_temp_adcFilt);        // 7: for board temperature calibration
	        setScopeChannel(7, (int16_t)board_temp_deg_c);          // 8: for verifying board temperature calibration
	        consoleScope();
	      }
	    #endif

	    // ####### FEEDBACK SERIAL OUT #######
	    #if defined(FEEDBACK_SERIAL_USART2) || defined(FEEDBACK_SERIAL_USART3)
	      if (main_loop_counter % 2 == 0) {    // Send data periodically every 10 ms
	        Feedback.start	        = (uint16_t)SERIAL_START_FRAME;
	        Feedback.cmd1           = (int16_t)cmd1;
	        Feedback.cmd2           = (int16_t)cmd2;
	        Feedback.speedR_meas	  = (int16_t)rtY_Right.n_mot;
	        Feedback.speedL_meas	  = (int16_t)rtY_Left.n_mot;
	        Feedback.batVoltage	    = (int16_t)(batVoltage * BAT_CALIB_REAL_VOLTAGE / BAT_CALIB_ADC);
	        Feedback.boardTemp	    = (int16_t)board_temp_deg_c;

	        #if defined(FEEDBACK_SERIAL_USART2)
	          if(__HAL_DMA_GET_COUNTER(huart2.hdmatx) == 0) {
	            Feedback.cmdLed         = (uint16_t)sideboard_leds_L;
	            Feedback.checksum       = (uint16_t)(Feedback.start ^ Feedback.cmd1 ^ Feedback.cmd2 ^ Feedback.speedR_meas ^ Feedback.speedL_meas
	                                               ^ Feedback.batVoltage ^ Feedback.boardTemp ^ Feedback.cmdLed);

	            HAL_UART_Transmit_DMA(&huart2, (uint8_t *)&Feedback, sizeof(Feedback));
	          }
	        #endif
	        #if defined(FEEDBACK_SERIAL_USART3)
	          if(__HAL_DMA_GET_COUNTER(huart3.hdmatx) == 0) {
	            Feedback.cmdLed         = (uint16_t)sideboard_leds_R;
	            Feedback.checksum       = (uint16_t)(Feedback.start ^ Feedback.cmd1 ^ Feedback.cmd2 ^ Feedback.speedR_meas ^ Feedback.speedL_meas
	                                               ^ Feedback.batVoltage ^ Feedback.boardTemp ^ Feedback.cmdLed);

	            HAL_UART_Transmit_DMA(&huart3, (uint8_t *)&Feedback, sizeof(Feedback));
	          }
	        #endif
	      }
	    #endif

	    // ####### POWEROFF BY POWER-BUTTON #######
	    poweroffPressCheck();

	    // ####### BEEP AND EMERGENCY POWEROFF #######
	    if ((TEMP_POWEROFF_ENABLE && board_temp_deg_c >= TEMP_POWEROFF && speedAvgAbs < 20) || (batVoltage < BAT_DEAD && speedAvgAbs < 20)) {  // poweroff before mainboard burns OR low bat 3
	      poweroff();
	    } else if (rtY_Left.z_errCode || rtY_Right.z_errCode) {                                           // 1 beep (low pitch): Motor error, disable motors
	      enable = 0;
	      beepCount(1, 24, 1);
	    } else if (timeoutFlagADC) {                                                                      // 2 beeps (low pitch): ADC timeout
	      beepCount(2, 24, 1);
	    } else if (timeoutFlagSerial) {                                                                   // 3 beeps (low pitch): Serial timeout
	      beepCount(3, 24, 1);
	    } else if (timeoutCnt > TIMEOUT) {                                                                // 4 beeps (low pitch): General timeout (PPM, PWM, Nunchuck)
	      beepCount(4, 24, 1);
	    } else if (TEMP_WARNING_ENABLE && board_temp_deg_c >= TEMP_WARNING) {                             // 5 beeps (low pitch): Mainboard temperature warning
	      beepCount(5, 24, 1);
	    } else if (BAT_LVL1_ENABLE && batVoltage < BAT_LVL1) {                                            // 1 beep fast (medium pitch): Low bat 1
	      beepCount(0, 10, 6);
	    } else if (BAT_LVL2_ENABLE && batVoltage < BAT_LVL2) {                                            // 1 beep slow (medium pitch): Low bat 2
	      beepCount(0, 10, 30);
	    } else if (BEEPS_BACKWARD && ((speed < -50 && speedAvg < 0) || MultipleTapBrake.b_multipleTap)) { // 1 beep fast (high pitch): Backward spinning motors
	      beepCount(0, 5, 1);
	      backwardDrive = 1;
	    } else {  // do not beep
	      beepCount(0, 0, 0);
	      backwardDrive = 0;
	    }


	    // ####### INACTIVITY TIMEOUT #######
	    if (abs(speedL) > 50 || abs(speedR) > 50) {
	      inactivity_timeout_counter = 0;
	    } else {
	      inactivity_timeout_counter++;
	    }
	    if (inactivity_timeout_counter > (INACTIVITY_TIMEOUT * 60 * 1000) / (DELAY_IN_MAIN_LOOP + 1)) {  // rest of main loop needs maybe 1ms
	      poweroff();
	    }

	    // HAL_GPIO_TogglePin(LED_PORT, LED_PIN);                 // This is to measure the main() loop duration with an oscilloscope connected to LED_PIN
	    // Update main loop states
	    lastSpeedL = speedL;
	    lastSpeedR = speedR;
	    main_loop_counter++;
	    timeoutCnt++;
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIGCONV_T3_TRGO;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 4;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_DUALMODE_REGSIMULT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */
  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 4;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_9;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
  htim1.Init.Period = 64000000 / 2 / PWM_FREQ;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_ENABLE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_SET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_SET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_ENABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_ENABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = DEAD_TIME;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_SlaveConfigTypeDef sSlaveConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 64000000 / 2 / PWM_FREQ;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sSlaveConfig.SlaveMode = TIM_SLAVEMODE_GATED;
  sSlaveConfig.InputTrigger = TIM_TS_ITR0;
  if (HAL_TIM_SlaveConfigSynchro(&htim3, &sSlaveConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_HalfDuplex_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(TPS_ENA_GPIO_Port, TPS_ENA_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LIGHT_GPIO_Port, LIGHT_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PWR_BTN_Pin */
  GPIO_InitStruct.Pin = PWR_BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PWR_BTN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : TPS_ENA_Pin */
  GPIO_InitStruct.Pin = TPS_ENA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TPS_ENA_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : HALL_C_Pin HALL_A_Pin HALL_B_Pin */
  GPIO_InitStruct.Pin = HALL_C_Pin|HALL_A_Pin|HALL_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LIGHT_Pin */
  GPIO_InitStruct.Pin = LIGHT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LIGHT_GPIO_Port, &GPIO_InitStruct);

  /*Configure peripheral I/O remapping */
  __HAL_AFIO_REMAP_PD01_ENABLE();

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/