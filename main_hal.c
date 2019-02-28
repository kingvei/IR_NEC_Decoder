/*
 * IR_NEC z CubeMx HAL
 *
 * W CubeMx ustawić
 * 1. Wejście kanalu 1 lub 2 ustawić jako wejście z podciąganiem do góry(wybrać wejście
 * FT w razie potrzeby)
 * 2. Slave mode Reset
 * 3. Trigger source TI1FP1 albo TI2FP2
 * 4. Clock source: internal
 * 5. Channel 1 lub 2 (w zależności od wybranego wejścia w p. 3) ustawić Input Capture direct mode
 * 6. Inny kanał ustawić jako Output Compare No Output
 * 7. Włączyć przerwanie globalne timera
 * 8. Preskaler taki by dla najdłuższego impulsu (czyli 97ms) licznik zliczyłm między 64000 a UINT16_MAX
 * 9. ARR == UINT16_MAX
 * 10. Input Capture polarity: Falling
 * 11. Input filter: ustawić w razie potrzeby
 * 12. Częstotliwość taktowania licznika nie jest krytyczna, można ustawić niższą
 *     jeśli potrzebne jest mocniejsze filtrowanie wejścia
 * 13. Output compare: frozen, pulse UINT16MAX
 */

//...

/* USER CODE BEGIN Includes */

#include "ir.h"


/* USER CODE END Includes */

/* USER protypes */
void	buf2str(uint16_t address, uint16_t command, bool repeat);
char * getbufptr(void);
int getbuflen(void);

//...
  /* USER CODE BEGIN 2 */

///Włącz oba wybrane kanały w odpowiednim trybie
HAL_TIM_IC_Start_IT(&htim2,TIM_CHANNEL_1);
HAL_TIM_OC_Start_IT(&htim2,TIM_CHANNEL_2);

  /* USER CODE END 2 */
//...

/* USER CODE BEGIN 4 */

//Odebrano kolejny impuls z pilota
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{

	//Odczytaj długość impulsu
  uint16_t captval = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

  //Zdekoduj impuls. Dekoder Wywoła callback jeśli odebrano całą komendę lub jej powtórzenie
  ir_decode( captval );

  //Zamigaj diodą że odebrano sygnal z pilota
  HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
}


//Wystąpilo przepełnienie licznika - zakończono nadawanie komendy
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* Prevent unused argument(s) compilation warning */
	//jeśli jest potrzebne rozrózńianie kanału to przez odpowiednie pole pod htim
  UNUSED(htim);

  //wyprowadź odebraną komendę na UART
  HAL_UART_Transmit_IT(&huart2,getbufptr(),getbuflen());

	//Zresetuj dekoder IR
	ir_reset();

	//Wyłącz diodę sygnalizacyjną
	HAL_GPIO_WritePin(LD2_GPIO_Port,LD2_Pin,GPIO_PIN_RESET);

}

//Odebrano komendę lub jej powtórzenie
void ir_callback(uint16_t address, uint16_t command, bool repeat)
{
	//zamień odebraną komendę na postać znakową
	buf2str(uint16_t address, uint16_t command, bool repeat);

}
/* USER CODE END 4 */

