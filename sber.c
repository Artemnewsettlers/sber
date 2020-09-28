/*
* Файл   			main.c
* Наименование		Проект драйвера приема/передачи (stream) байтовых пакетов переменной длины (тестовое задание)
* Автор				Новосельцев Артем
* Платформа			МК 1986ВЕ91T (Миландр)
* Дата				28.09.2020
*/


/*
 *Определение команд протокола
*/
#define CAN
#define ID				(58<<17)				// индентификатор ':' << 17 , читаются 18-28 бит (для МК 1986ВЕ91T)


uint8_t receivUart[16]	= {0};					//буфер для приема UART
uint8_t	dataBuffer[16] 	= {0};					//буфер для работы с принятыми данными
uint8_t transmUart[16] 	= {0};					//буфер для формирование пакета UART на отправку

uint8_t checkSumm;								//контрольная сумма
uint8_t rxFlag;									//флаг приема данных	
uint8_t dataFlag 	= 0;						//флаг обработки данных
uint8_t lght;									//длина поля данных (в байтах)


/*
 *прототипы функций
*/
void Receiver(void);
void Transmiter(void);
uint8_t CalcCS(uint8_t *buf, int amount);
uint8_t СheckCS (uint8_t *buf, int amount);



int main(void)
{
	

	while(1)
	{	
		
		//ждем в цикле  пока установитс флаг по приему данных
		if (rxFlag)								
		{
			//прием сообщений 
			Receiver();			
			//отправка сообщений
			Transmiter();		
		}

	}
}

/*
 *Прием сообщений
*/
void Receiver(void)
{

#ifdef CAN
	/*
	Структура пакета CAN

	rx_frame.ID 		= 0x00; 				// Идентификатор кадра данных 
	rx_frame.PRIOR_0 	= 0x00; 				// Приоритет кадра данных 
	rx_frame.IDE 		= 0x00; 				// Формат кадра данных
	rx_frame.DLC 		= 0x00; 				// Длина поля данных
	rx_frame.Data[0] 	= 0x00000000; 			// Первые четыре байта данных 
	rx_frame.Data[1] 	= 0x00000000; 			// Вторые четыре байта данных
	*/

	//Извлекаем данные из приемного буфера в структуру rx_frame
	CAN_GetRawReceivedData(MDR_CAN1, CAN_BUFFER_0, &rx_frame);

	//Проверим тип заголовка сообщения
	if (rx_frame.IDE == CAN_ID_STD) 			//формат кадра данных - стандартный		
	{
		//считываем идентификатор кадра данных
		if (rx_frame.ID == ID) 					//индентификатор ':'
		{   
		 	//записали длину поля данных (в байтах)
			lght = rx_frame.DLC;
			
			//переписывааем принятые данные (8 байт) в буфер для дальнейшей обработки
			//Первые четыре байта данных
			dataBuffer[0] = (uint8_t) (rx_frame.Data[0] & 0x000000FF); 
			dataBuffer[1] = (uint8_t) ((rx_frame.Data[0] & 0x0000FF00) >> 8); 
			dataBuffer[2] = (uint8_t) ((rx_frame.Data[0] & 0x00FF0000) >> 16);
			dataBuffer[3] = (uint8_t) ((rx_frame.Data[0] & 0xFF000000) >> 24);
		
			// Вторые четыре байта данных
			dataBuffer[4] = (uint8_t) (rx_frame.Data[1] & 0x000000FF);
			dataBuffer[5] = (uint8_t) ((rx_frame.Data[1] & 0x0000FF00) >> 8);
			dataBuffer[6] = (uint8_t) ((rx_frame.Data[1] & 0x00FF0000) >> 16);
			dataBuffer[7] = (uint8_t) ((rx_frame.Data[1] & 0xFF000000) >> 24);

			//проверка контрольной суммы
			if (СheckCS( dataBuffer, sizeof (dataBuffer)))	 dataFlag = 1;		//выставляем флаг, данные приняты

		}
	}

#else //UART
	
	/*
	Структура пакета UART

	receivUart[0]  	= 	':'  	- разгранич пакетов
	receivUart[1]	= 	lght 	- длина поля данных (в байтах)
	receivUart[...]	=	DATA 	- байты данных
	receivUart[]	= 	CRC 	- контрол сумма
	*/

	uint8_t i, k = 0;

	//обнуляем буфер receivUart
	for (i=0; i<16; i++)								
	{
		receivUart[i] = 0;
	}

	//перепис из буфера FIFO UART в буфер receivUart , пока не считаем весь буфер FIFO
	do	 
	{	
		receivUart[k] = UART_ReceiveData(MDR_UART1);					  					
		k++;
				
	} while(!(MDR_UART1 -> FR & 0x10));						//пока статус буфера не изменится на "пустой"

	//проверка контрольной суммы и символа разделителя пакетов ':'
	if (СheckCS( receivUart, sizeof (receivUart)) && receivUart[0] == ':') 
	{
		//записали длину поля данных (в байтах)
		lght = receivUart[1];								 

		//переписывааем принятые данные в буфер для дальнейшей обработки
		for (i=0; i<lght; i++)						
		{
			dataBuffer[i] = receivUart[i+2];
		}	

		dataFlag = 1;										//выставляем флаг, данные приняты	
			
	}

#endif

	rxFlag = 0;						
}


/*
 *Отправка сообщений в шину
*/
void Transmiter(void)
{

	uint8_t i;

#ifdef CAN

	//если, выставлен флаг данные приняты.
	if (dataFlag == 1) 
	{
		// Формирование кадра данных на отправку
		tx_frame.ID 		= ID; 					// Идентификатор кадра данных 
		tx_frame.PRIOR_0 	= 1; 					// Приоритет кадра данных 
		tx_frame.IDE 		= CAN_ID_STD; 			// Формат кадра данных - стандартный
		tx_frame.DLC 		= lght; 				// Длина поля данных (в байтах) 
		
		for (i=0; i < lght; i++)		
		{
			//инкрементируем полученные данные 
			dataBuffer[i] = dataBuffer[i] + 1;
		}

		//расчет контрольной суммы
		dataBuffer[lght+1] = CalcCS(dataBuffer, (sizeof (dataBuffer)-1)); 

		//Первые четыре байта данных на отправку
		tx_frame.Data[0] = dataBuffer[0]; 				//записали мл. байт
		tx_frame.Data[0] += dataBuffer[1] * 256;
		tx_frame.Data[0] += dataBuffer[2] * 65536;
		tx_frame.Data[0] += dataBuffer[3] * 16777216;

		// Вторые четыре байта данных на отправку
		tx_frame.Data[1] = dataBuffer[0]; 				//записали мл. байт
		tx_frame.Data[1] += dataBuffer[1] * 256;
		tx_frame.Data[1] += dataBuffer[2] * 65536;
		tx_frame.Data[1] += dataBuffer[3] * 16777216;

		// Передача кадра 
		CAN_Transmit(MDR_CAN1,  CAN_BUFFER_0, &tx_frame);

		dataFlag == 0;

	}

#else //UART
	
	/*
	 *формирование пакета UART на отправку

	transmUart[0]  	= 	':'  	- разгранич пакетов
	transmUart[1]	= 	lght 	- длина поля данных (в байтах)
	transmUart[...]	=	DATA+1 	- инкрементируем полученные данные 
	transmUart[]	= 	CRC 	- контрол сумма
	*/

	//если, выставлен флаг данные приняты.
	if (dataFlag == 1) 
	{
		//формируем пакет на отправку
		transmUart[0] = ':';  					//записываем индентификатор ':'
		transmUart[1] = lght; 					//длина поля данных

		for (i=0; i < lght; i++)		
		{
			//инкрементируем полученные данные 
			transmUart[i+2] = dataBuffer[i] + 1;
		}

		//расчет контрольной суммы
		transmUart[2+lght+1] = CalcCS(transmUart, (sizeof (transmUart)-1)); 
			
		//отправляем пакет данных
		for(int i = 0; i < (sizeof (transmUart)); i++)
		{
			UART_SendData(MDR_UART1, transmUart[i]);
		}
			
		dataFlag == 0;
	}

#endif

}


/*
 *Проверка контрольной суммы, amount - полная длина буфера
*/
uint8_t СheckCS (uint8_t *buf, int amount )
{
	char summ = 0;

	for ( int i = 0; i < amount; i++ )
	{
		summ += buf[i];
   	}
       
	return summ?FALSE:TRUE;		//summ = 1 -> ложь

}

/*
 *Расчет контрольной суммы: сложение затем 0 - сумма
 *buf - ук-ль на данные, amount - количество байт для расчета (длина буфера - 1)
*/
uint8_t CalcCS(uint8_t *buf, int amount )
{
	char summ = 0;
	for ( int i = 0; i < amount; i++ )
	{
		summ += buf[i];
   	}
       
	return (unsigned char) (0 - summ);	
}

/*
 *Обработчик прерывания CAN
*/
void CAN1_IRQHandler(void)
{
	//проверка установки флага прерывания по окончании приема данных
	if(CAN_GetRxITStatus(MDR_CAN1, CAN_BUFFER_0) == SET)
	{
		//очистка флага прерывания 
		CAN_ITClearRxTxPendingBit(MDR_CAN1,CAN_BUFFER_0, CAN_STATUS_RX_READY)
		
		//Уствновка флага према
		rxFlag = SET;

	}

}

/*
 *Обработчик прерывания UART
 *Прерывания по окончании приема данных по таймауту.
*/
void UART1_IRQHandler(void)												
{	
	//проверка установки флага прерывания по окончании приема данных
	if (UART_GetITStatusMasked(MDR_UART1, UART_IT_RT) == SET) 			
	{
		//очистка флага прерывания
		UART_ClearITPendingBit(MDR_UART1, UART_IT_RT);					 
		
		//Уствновка флага према
		rxFlag = SET;  
	}

}
