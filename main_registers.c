/*
 *	Ustawienia po rejstrach ORAZ dynamiczne wł/wyłączanie timera
 */

/* USER CODE BEGIN Includes */


#include "ir.h"
#include "gpio_config.h"

/* USER CODE END Includes */

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

void ir_init(void);

/* USER CODE END PFP */

//..
  /* USER CODE BEGIN 2 */

  ir_init();	//init timera 2 oraz zremapowanego wejścia Ch1

  NVIC_EnableIRQ(TIM2_IRQn);

  /* USER CODE END 2 */
//..

/* USER CODE BEGIN 4 */

void ir_init(void)
{
	//TIM2 kanał 5 pin alternatywny PA15 (jedyny FT na porcie A)
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;
	gpio_pin_cfg(GPIOA, PA15, gpio_mode_input_pull);
	GPIOA->ODR |= GPIO_ODR_ODR15; //PA15) = 1;
	//remaping wejścia 1 (FT!)
	AFIO->MAPR |= AFIO_MAPR_TIM2_REMAP_0;

	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

	//kanał pierwszy łapie impulsy z wejścia oraz resetuje licznik
	TIM2->CCMR1 = TIM_CCMR1_CC1S_0;

	//filtrowanie
	//obniżenie częstotliwości samplowania
	TIM2->CR1 = TIM_CR1_CKD_1;	//obniżenie częstotliwości samplowania 1/4 CK_INT
	//fdts/32, n=8 co odfiltrowuje impulsy krótsze niż 16µs. Wydłużenie tego czasu wymaga obniżenia
	//częśtotliwości taktowania licznika
	TIM2->CCMR1 |= TIM_CCMR1_IC1F_3|TIM_CCMR1_IC1F_2|TIM_CCMR1_IC1F_1|TIM_CCMR1_IC1F_0;

	TIM2->CCER = TIM_CCER_CC1P; //falling edge, polarity inverted
	//tu ewentualny preskaler w CCMR1_IC1PS
	TIM2->CCER |= TIM_CCER_CC1E; //capture enabled
	TIM2->DIER = TIM_DIER_CC1IE /*| TIM_DIER_UIE*/; //przerwania compare/update
	//reset trigger dla input 1
	TIM2->SMCR = TIM_SMCR_SMS_2 | TIM_SMCR_TS_2 | TIM_SMCR_TS_0;

	//kanał drugi generuje przerwania od przepełnienia
	TIM2->CCR2 = UINT16_MAX;
	TIM2->DIER |= TIM_DIER_CC2IE; //przerwanie ustawia tryb wyzwalania: trigger


	TIM2->PSC = 97;	//<-- musi grać z wartością w ir_conf.h!!!
	TIM2->ARR = UINT16_MAX;


	//TIM2->CR1 = TIM_CR1_CEN /*| TIM_CR1_OPM*/;

	//zamiast ciągłej pracy timer będzie włączany w przerwaniu exti
	//pin PA15
	AFIO->EXTICR[4] = AFIO_EXTICR4_EXTI15_PA;
	EXTI->IMR = EXTI_IMR_IM15;	//interrupt request
	EXTI->FTSR = EXTI_FTSR_FT15;	//zbocze opadające

	NVIC_EnableIRQ(EXTI15_10_IRQn);


}


void EXTI15_10_IRQHandler(void) {

	if( EXTI->PR & EXTI_PR_PR15)
	{
		EXTI->PR = EXTI_PR_PR15;

		//włączamy timer
		TIM2->CR1 = TIM_CR1_CEN;
		//ir_capture(0);	//zmiana stanu na WAIT_FOR_HEAD dla pierwszego impulsu
		//wygląda na to, że po włączeniu timera od razu odpala się przerwanie od capture

		//wersja z timerem pracującym na okrągło wydaje się łatwiejszym przypadkiem do analizy
		//a tym samym bardziej pewnym rozwiązaniem

	}

}

void TIM2_IRQHandler(void)
{

	if(TIM2->SR & TIM_SR_CC1IF)	//łapanie impulsów wejściowych
	{
			TIM2->SR &= ~TIM_SR_CC1IF;

			//jeśli wystąpiła flaga overcapture
			if(TIM2->SR & TIM_SR_CC1OF)
			{
					//kasujemy flagę
					TIM2->SR &= ~TIM_SR_CC1OF;

					//resetujemy dekoder
					ir_reset();
			}

			//zdekoduj puls
			ir_decode( TIM2->CCR1);

			HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
	}

		if(TIM2->SR & TIM_SR_CC2IF)	//łapanie przepełnienia licznika
		{
				//kasowanie flagi przerwania
				TIM2->SR &= ~TIM_SR_CC2IF;


				//ir_reset() nie jest potrzebne

				//wyłączamy timer
				TIM2->CR1 = 0;
				//TIM2->CNT = 0;

				HAL_GPIO_WritePin(LD2_GPIO_Port,LD2_Pin,GPIO_PIN_RESET);
		}


}

void ir_callback(uint16_t address, uint16_t command, bool repeat)
{
	//wykonaj komendę
}


/* USER CODE END 4 */

