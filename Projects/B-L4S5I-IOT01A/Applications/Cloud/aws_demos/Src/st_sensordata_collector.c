/* MIT License

Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */

#include <aws_customdemo_inc.h>
#include "st_sensordata_collector.h"

#include "main.h"
#include "stm32l475e_iot01_accelero.h"
#include "stm32l475e_iot01_psensor.h"
#include "stm32l475e_iot01_gyro.h"
#include "stm32l475e_iot01_hsensor.h"
#include "stm32l475e_iot01_tsensor.h"
#include "stm32l475e_iot01_magneto.h"
#include "vl53l0x_api.h"
#include "ai_platform.h"
#include "network.h"
#include "network_data.h"

#define TEMPERATURE_TASK_READ_DELAY_MS  3000

typedef enum {
	LONG_RANGE 		= 0, /*!< Long range mode */
	HIGH_SPEED 		= 1, /*!< High speed mode */
	HIGH_ACCURACY	= 2, /*!< High accuracy mode */
} RangingConfig_e;

extern I2C_HandleTypeDef hI2cHandler;

VL53L0X_Dev_t VL53L0XDev;
VL53L0X_RangingMeasurementData_t RangingMeasurementData;
int LeakyFactorFix8 = (int)( 0.6 *256);

ai_handle network;
float aiInData[AI_NETWORK_IN_1_SIZE];
float aiOutData[AI_NETWORK_OUT_1_SIZE];
uint8_t activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];
const char* activities[AI_NETWORK_OUT_1_SIZE] = {
  "stationary", "walking", "running"
};

int BSP_Proximity_Init();
int SetupSingleShot(RangingConfig_e rangingConfig);
float BSP_Proximity_Read();
void Sensor_SetNewRange(VL53L0X_Dev_t *pDev, VL53L0X_RangingMeasurementData_t *pRange);
static void AI_Init(ai_handle w_addr, ai_handle act_addr);
static void AI_Run(float *pIn, float *pOut);
static uint32_t argmax(const float *values, uint32_t len);

static void prvSensorsInit(void) {
  
  if (HSENSOR_OK != BSP_HSENSOR_Init()) {
    Error_Handler();
  }
  
  if (TSENSOR_OK != BSP_TSENSOR_Init()) {
    Error_Handler();
  }
  
  if (PSENSOR_OK != BSP_PSENSOR_Init()) {
    Error_Handler();
  }
  
  if (MAGNETO_OK != BSP_MAGNETO_Init()) {
    Error_Handler();
  }
  
  if (GYRO_OK != BSP_GYRO_Init()) {
    Error_Handler();
  }
  
  if (ACCELERO_OK != BSP_ACCELERO_Init()) {
    Error_Handler();
  }
  
  if (0 != BSP_Proximity_Init()) {
    Error_Handler();
  }
}

static void AI_Init(ai_handle w_addr, ai_handle act_addr)
{
  ai_error err;

  /* 1 - Create an instance of the model */
  err = ai_network_create(&network, AI_NETWORK_DATA_CONFIG);
  if (err.type != AI_ERROR_NONE) {
    printf("ai_network_create error - type=%d code=%d\r\n", err.type, err.code);
    Error_Handler();
  }

  /* 2 - Initialize the instance */
  const ai_network_params params = {
      AI_NETWORK_DATA_WEIGHTS(w_addr),
      AI_NETWORK_DATA_ACTIVATIONS(act_addr)
  };

  if (!ai_network_init(network, &params)) {
    err = ai_network_get_error(network);
    printf("ai_network_init error - type=%d code=%d\r\n", err.type, err.code);
    Error_Handler();
  }
}

static void AI_Run(float *pIn, float *pOut)
{
  ai_i32 batch;
  ai_error err;

  /* 1 - Create the AI buffer IO handlers with the default definition */
  ai_buffer ai_input[AI_NETWORK_IN_NUM] = AI_NETWORK_IN;
  ai_buffer ai_output[AI_NETWORK_OUT_NUM] = AI_NETWORK_OUT;

  /* 2 - Update IO handlers with the data payload */
  ai_input[0].n_batches = 1;
  ai_input[0].data = AI_HANDLE_PTR(pIn);
  ai_output[0].n_batches = 1;
  ai_output[0].data = AI_HANDLE_PTR(pOut);

  batch = ai_network_run(network, ai_input, ai_output);
  if (batch != 1) {
    err = ai_network_get_error(network);
    printf("AI ai_network_run error - type=%d code=%d\r\n", err.type, err.code);
    Error_Handler();
  }
}

static uint32_t argmax(const float * values, uint32_t len)
{
  float max_value = values[0];
  uint32_t max_index = 0;
  for (uint32_t i = 1; i < len; i++) {
    if (values[i] > max_value) {
      max_value = values[i];
      max_index = i;
    }
  }
  return max_index;
}

/* This task read the sensor data from the STM32 target and publishes to the telemetry queue
 *
 */
void onboardSensorReaderTask(void *mode) {



	//IotMqttConnection_t mqttConnection = gpmqttConnection;

	//IotMqttPublishInfo_t publishInfo = IOT_MQTT_PUBLISH_INFO_INITIALIZER;
	//IotMqttCallbackInfo_t publishComplete = IOT_MQTT_CALLBACK_INFO_INITIALIZER;


	char *pSensorPayload;

//	intptr_t publishCount = 0;
	//IotMqttError_t publishStatus = IOT_MQTT_STATUS_PENDING;

	int j = 0;
	//float temperatureC;
	float TEMPERATURE_Value;
	float HUMIDITY_Value;
	float PRESSURE_Value;
	int16_t ACC_Value[3];
	float GYR_Value[3];
	int16_t MAG_Value[3];
        float PROXIMITY_Value;

	int snprintfreturn = 0;
	uint32_t motion_pose = 0;

	gucSensorTopicName[31] = '\0';

	IotLogInfo("ENTRY : onboardSensorReaderTask, mode: %d ", *(int *)mode);
	prvSensorsInit();
	AI_Init(ai_network_data_weights_get(), activations);

	uint32_t write_index = 0;
	do {
		vTaskDelay(pdMS_TO_TICKS(TEMPERATURE_TASK_READ_DELAY_MS));

		TEMPERATURE_Value = BSP_TSENSOR_ReadTemp();
		HUMIDITY_Value = BSP_HSENSOR_ReadHumidity();
		PRESSURE_Value = BSP_PSENSOR_ReadPressure();
		BSP_ACCELERO_AccGetXYZ(ACC_Value);
		BSP_GYRO_GetXYZ(GYR_Value);
		BSP_MAGNETO_GetXYZ(MAG_Value);
                PROXIMITY_Value = BSP_Proximity_Read();
                
		IotLogInfo("Accelerometer [X: %d] ", ACC_Value);

		/* Normalize data to [-1; 1] and accumulate into input buffer */
		/* Note: window overlapping can be managed here */
		aiInData[write_index + 0] = (float) ACC_Value[0] / 4000.0f;
		aiInData[write_index + 1] = (float) ACC_Value[1] / 4000.0f;
		aiInData[write_index + 2] = (float) ACC_Value[2] / 4000.0f;
		write_index += 3;

		if (write_index == AI_NETWORK_IN_1_SIZE) {
			write_index = 0;
			IotLogInfo("AI> Running inference\r\n");
			AI_Run(aiInData, aiOutData);

			/* Output results */
			for (uint32_t i = 0; i < AI_NETWORK_OUT_1_SIZE; i++) {
				IotLogInfo("AI> %8.6f ", aiOutData[i]);
			}
			uint32_t class = argmax(aiOutData, AI_NETWORK_OUT_1_SIZE);
			motion_pose = class;
			IotLogInfo("AI> : %d - %s\r\n", (int) class, activities[class]);
		} else {
			IotLogInfo("AI> not executed... %d \r\n", write_index);
		}

		pSensorPayload = pvPortMalloc(sizeof(char) * SENSOR_STATUS_MSG_BUF_LEN);

		/* Format data for transmission to AWS */
		snprintfreturn = snprintf(pSensorPayload, SENSOR_STATUS_MSG_BUF_LEN,
				"{\"Board_id\":\"%s\","
						"\"Temp\": %d, \"Hum\": %d, \"Press\": %d, "
						"\"Accel_X\": %d, \"Accel_Y\": %d, \"Accel_Z\": %d, \"Motion_Pose\": %d,"
						"\"Gyro_X\": %d, \"Gyro_Y\": %d, \"Gyro_Z\": %d, "
						"\"Magn_X\": %d, \"Magn_Y\": %d, \"Magn_Z\": %d, \"Proxi\": %d"
						"}", "st-discovery-board-01", (int) TEMPERATURE_Value,
				(int) HUMIDITY_Value, (int) PRESSURE_Value, ACC_Value[0],
				ACC_Value[1], ACC_Value[2], (int)motion_pose, (int) GYR_Value[0],
				(int) GYR_Value[1], (int) GYR_Value[2], MAG_Value[0],
				MAG_Value[1], MAG_Value[2], (int) PROXIMITY_Value);

		IotLogInfo(
				"Publishing sensor data as json string: %s of length [ %d]\n",
				pSensorPayload, snprintfreturn);

	   if ( *(int *)mode == 1 ) {
       sendToTelemetryQueue(gucSensorTopicName, pSensorPayload, snprintfreturn);
     } else {
       IotLogInfo("Skipped sendToTelemetryQueue");
     }

	} while (j++ < SENSOR_DATA_NUM_POLL_CYCLE);


}

int BSP_Proximity_Init()
{
  RangingConfig_e RangingConfig = LONG_RANGE;
  uint16_t Id;
  int status;
    
  VL53L0XDev.I2cHandle = &hI2cHandler;
  
  VL53L0X_Dev_t *pDev;
  pDev = &VL53L0XDev;
  pDev->I2cDevAddr = 0x52;
  pDev->Present = 0;
  HAL_Delay(2);
  
  /* Set VL53L0X API trace level */
  VL53L0X_trace_config(NULL, TRACE_MODULE_NONE, TRACE_LEVEL_NONE, TRACE_FUNCTION_NONE); // No Trace
  
  status = VL53L0X_WrByte(pDev, 0x88, 0x00);
  
  /* Try to read one register using default 0x52 address */
  status = VL53L0X_RdWord(pDev, VL53L0X_REG_IDENTIFICATION_MODEL_ID, &Id);
  
  if (status) {
    IotLogInfo("VL53L0X Read id fail\n");
  }
  
  if (Id == 0xEEAA) {
    status = VL53L0X_DataInit(pDev);
    if( status == 0 ){
      pDev->Present = 1;
      status = SetupSingleShot(RangingConfig);
    }
    else{
      IotLogInfo("VL53L0X_DataInit fail\n");
    }
  }
  else {
    IotLogInfo("unknown ID %x\n", Id);
    status = 1;
  }
  
  return status;
}

int SetupSingleShot(RangingConfig_e rangingConfig){

  int status;
  uint8_t VhvSettings;
  uint8_t PhaseCal;
  uint32_t refSpadCount;
  uint8_t isApertureSpads;
  FixPoint1616_t signalLimit = (FixPoint1616_t)(0.25*65536);
  FixPoint1616_t sigmaLimit = (FixPoint1616_t)(18*65536);
  uint32_t timingBudget = 33000;
  uint8_t preRangeVcselPeriod = 14;
  uint8_t finalRangeVcselPeriod = 10;
  
  status=VL53L0X_StaticInit(&VL53L0XDev);
  if( status ){
    IotLogInfo("VL53L0X_StaticInit failed\n");
  }
  
  status = VL53L0X_PerformRefCalibration(&VL53L0XDev, &VhvSettings, &PhaseCal);
  if( status ){
    IotLogInfo("VL53L0X_PerformRefCalibration failed\n");
  }
  
  status = VL53L0X_PerformRefSpadManagement(&VL53L0XDev, &refSpadCount, &isApertureSpads);
  if( status ){
    IotLogInfo("VL53L0X_PerformRefSpadManagement failed\n");
  }
  
  status = VL53L0X_SetDeviceMode(&VL53L0XDev, VL53L0X_DEVICEMODE_SINGLE_RANGING); // Setup in single ranging mode
  if( status ){
    IotLogInfo("VL53L0X_SetDeviceMode failed\n");
  }
  
  status = VL53L0X_SetLimitCheckEnable(&VL53L0XDev, VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE, 1); // Enable Sigma limit
  if( status ){
    IotLogInfo("VL53L0X_SetLimitCheckEnable failed\n");
  }
  
  status = VL53L0X_SetLimitCheckEnable(&VL53L0XDev, VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 1); // Enable Signa limit
  if( status ){
    IotLogInfo("VL53L0X_SetLimitCheckEnable failed\n");
  }
  /* Ranging configuration */
  switch(rangingConfig) {
  case LONG_RANGE:
    signalLimit = (FixPoint1616_t)(0.1*65536);
    sigmaLimit = (FixPoint1616_t)(60*65536);
    timingBudget = 33000;
    preRangeVcselPeriod = 18;
    finalRangeVcselPeriod = 14;
    break;
  case HIGH_ACCURACY:
    signalLimit = (FixPoint1616_t)(0.25*65536);
    sigmaLimit = (FixPoint1616_t)(18*65536);
    timingBudget = 200000;
    preRangeVcselPeriod = 14;
    finalRangeVcselPeriod = 10;
    break;
  case HIGH_SPEED:
    signalLimit = (FixPoint1616_t)(0.25*65536);
    sigmaLimit = (FixPoint1616_t)(32*65536);
    timingBudget = 20000;
    preRangeVcselPeriod = 14;
    finalRangeVcselPeriod = 10;
    break;
  default:
    IotLogInfo("Not Supported");
  }
  
  status = VL53L0X_SetLimitCheckValue(&VL53L0XDev,  VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, signalLimit);
  if( status ){
    IotLogInfo("VL53L0X_SetLimitCheckValue failed\n");
  }
  
  status = VL53L0X_SetLimitCheckValue(&VL53L0XDev,  VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE, sigmaLimit);
  if( status ){
    IotLogInfo("VL53L0X_SetLimitCheckValue failed\n");
  }
  
  status = VL53L0X_SetMeasurementTimingBudgetMicroSeconds(&VL53L0XDev,  timingBudget);
  if( status ){
    IotLogInfo("VL53L0X_SetMeasurementTimingBudgetMicroSeconds failed\n");
  }
  
  status = VL53L0X_SetVcselPulsePeriod(&VL53L0XDev,  VL53L0X_VCSEL_PERIOD_PRE_RANGE, preRangeVcselPeriod);
  if( status ){
    IotLogInfo("VL53L0X_SetVcselPulsePeriod failed\n");
  }
  
  status = VL53L0X_SetVcselPulsePeriod(&VL53L0XDev,  VL53L0X_VCSEL_PERIOD_FINAL_RANGE, finalRangeVcselPeriod);
  if( status ){
    IotLogInfo("VL53L0X_SetVcselPulsePeriod failed\n");
  }
  
  status = VL53L0X_PerformRefCalibration(&VL53L0XDev, &VhvSettings, &PhaseCal);
  if( status ){
    IotLogInfo("VL53L0X_PerformRefCalibration failed\n");
  }
  
  VL53L0XDev.LeakyFirst=1;
  
  return status;
}


float BSP_Proximity_Read()
{
  int status;
  float value;
  status = VL53L0X_PerformSingleRangingMeasurement(&VL53L0XDev,&RangingMeasurementData);
  if( status ==0 ){
    /* Push data logging to UART */
    Sensor_SetNewRange(&VL53L0XDev,&RangingMeasurementData);
  }
  else{
    Error_Handler();
  }
  
  if( RangingMeasurementData.RangeStatus == 0 ){
    value = (float)VL53L0XDev.LeakyRange/10;
  }
  else{
    value = 0;
  }
  
  return value;
}

/* Store new ranging data into the device structure, apply leaky integrator if needed */
void Sensor_SetNewRange(VL53L0X_Dev_t *pDev, VL53L0X_RangingMeasurementData_t *pRange){
  if( pRange->RangeStatus == 0 ){
    if( pDev->LeakyFirst ){
      pDev->LeakyFirst = 0;
      pDev->LeakyRange = pRange->RangeMilliMeter;
    }
    else{
      pDev->LeakyRange = (pDev->LeakyRange*LeakyFactorFix8 + (256-LeakyFactorFix8)*pRange->RangeMilliMeter)>>8;
    }
  }
  else{
    pDev->LeakyFirst = 1;
  }
}
