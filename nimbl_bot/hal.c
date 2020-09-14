#include "hal.h"
#include "reception.h"
#include "context.h"

#include "stm32g4xx_ll_usart.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
CRC_HandleTypeDef hcrc;

/*******************************************************************************
 * Function
 ******************************************************************************/
void HAL_EnableTxLockDetection(void);
void HAL_DisableTxLockDetection(void);

/**
 * \fn void USART3_IRQHandler(void)
 * \brief This function handles USART3 global interrupt
 *
 */
void USART3_IRQHandler(void)
{
    // check if we receive a data
    if ((LL_USART_IsActiveFlag_RXNE(USART3) != RESET) && (LL_USART_IsEnabledIT_RXNE(USART3) != RESET))
    {
        uint8_t data = LL_USART_ReceiveData8(USART3);
        ctx.data_cb(&data); // send reception byte to state machine
    }
    // Check if a timeout on reception occure
    if ((LL_USART_IsActiveFlag_RTO(USART3) != RESET) && (LL_USART_IsEnabledIT_RTO(USART3) != RESET))
    {
        if (ctx.tx_lock)
        {
            timeout();
        }
        else
        {
            //ERROR
        }
        LL_USART_ClearFlag_RTO(USART3);
        LL_USART_SetRxTimeout(USART3, TIMEOUT_VAL * (8 + 1 + 1));
    }
    USART3->ICR = 0XFFFFFFFF;
}

char HAL_is_tx_lock(void)
{
    if (ctx.tx_lock)
    {
        return 1;
    }
    else
    {
        return (READ_BIT(USART3->ISR, USART_ISR_BUSY) == (USART_ISR_BUSY));
    }
}

/**
 * \fn HAL_EnableTxLockDetection(void)
 * \brief PTP interrupt management
 */
void HAL_EnableTxLockDetection(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = ROBUS_TX_DETECT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(ROBUS_TX_DETECT_Port, &GPIO_InitStruct);
}
/**
 * \fn HAL_DisableTxLockDetection(void)
 * \brief PTP interrupt management
 */
void HAL_DisableTxLockDetection(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = ROBUS_TX_DETECT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(ROBUS_TX_DETECT_Port, &GPIO_InitStruct);
}
/**
 * \fn HAL_is_tx_lock(void)
 * \brief PTP interrupt management
 */
void HAL_LockTx(uint8_t lock)
{
	ctx.tx_lock = lock;
}

/**
 * \fn HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
 * \brief PTP interrupt management
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == ROBUS_PTPA_Pin)
    {
        ptp_handler(BRANCH_A);
        return;
    }
    if (GPIO_Pin == ROBUS_PTPB_Pin)
    {
        ptp_handler(BRANCH_B);
        return;
    }
    if (GPIO_Pin == ROBUS_TX_DETECT_Pin)
    {
        HAL_LockTx(true);
        return;
    }
}
/**
 * \fn unsigned char crc(unsigned char* data, unsigned char size)
 * \brief generate a CRC
 *
 * \param *data data table
 * \param size data size
 *
 * \return CRC value
 */
void set_baudrate(unsigned int baudrate)
{
    __HAL_RCC_USART3_CLK_ENABLE();

    LL_USART_InitTypeDef USART_InitStruct;

    // Initialise USART3
    LL_USART_Disable(USART3);
    USART_InitStruct.PrescalerValue = LL_USART_PRESCALER_DIV1;
    USART_InitStruct.BaudRate = baudrate;
    USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
    USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
    USART_InitStruct.Parity = LL_USART_PARITY_NONE;
    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;
    while(LL_USART_Init(USART3, &USART_InitStruct) != SUCCESS);
    LL_USART_Enable(USART3);

    // Enable Reception interrupt
    LL_USART_EnableIT_RXNE(USART3);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    HAL_NVIC_SetPriority(USART3_IRQn, 0, 1);
}
/**
 * \fn unsigned char crc(unsigned char* data, unsigned char size)
 * \brief generate a CRC
 *
 * \param *data data table
 * \param size data size
 *
 * \return CRC value
 */
void crc_init(void)
{
    __HAL_RCC_CRC_CLK_ENABLE();
    hcrc.Instance = CRC;
    hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_DISABLE;
    hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
    hcrc.Init.GeneratingPolynomial = 7;
    hcrc.Init.CRCLength = CRC_POLYLENGTH_16B;
    hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
    hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
    hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
    if (HAL_CRC_Init(&hcrc) != HAL_OK)
    {
    while(1);
    }
    if (HAL_CRCEx_Init(&hcrc) != HAL_OK)
    {
      while(1);
    }
}
/**
 * \fn unsigned char crc(unsigned char* data, unsigned char size)
 * \brief generate a CRC
 *
 * \param *data data table
 * \param size data size
 *
 * \return CRC value
 */
void crc(unsigned char *data, unsigned short size, unsigned char *crc)
{
	CRC_HandleTypeDef hcrc;

    unsigned short calc;
    if (size > 1)
    {
        calc = (unsigned short)HAL_CRC_Calculate(&hcrc, (uint32_t *)data, size);
    }
    else
    {
        calc = (unsigned short)HAL_CRC_Accumulate(&hcrc, (uint32_t *)data, 1);
    }
    crc[0] = (unsigned char)calc;
    crc[1] = (unsigned char)(calc >> 8);
}

/**
 * \fn reverse_detection(void)
 * \brief
 */
void reverse_detection(branch_t branch)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;

    if (branch == BRANCH_A)
    {
        HAL_GPIO_DeInit(ROBUS_PTPA_GPIO_Port, ROBUS_PTPA_Pin);
        __HAL_GPIO_EXTI_CLEAR_IT(ROBUS_PTPA_Pin);
        GPIO_InitStruct.Pin = ROBUS_PTPA_Pin;
        HAL_GPIO_Init(ROBUS_PTPA_GPIO_Port, &GPIO_InitStruct);
    }
    else if (branch == BRANCH_B)
    {
        HAL_GPIO_DeInit(ROBUS_PTPB_GPIO_Port, ROBUS_PTPB_Pin);
        __HAL_GPIO_EXTI_CLEAR_IT(ROBUS_PTPB_Pin);
        GPIO_InitStruct.Pin = ROBUS_PTPB_Pin;
        HAL_GPIO_Init(ROBUS_PTPB_GPIO_Port, &GPIO_InitStruct);
    }
}
/**
 * \fn reverse_detection(void)
 * \brief
 */
void set_PTP(branch_t branch)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; // Clean edge/state detection and set the PTP pin as output
    GPIO_InitStruct.Pull = GPIO_NOPULL;

    if (branch == BRANCH_A)
    {
        HAL_GPIO_DeInit(ROBUS_PTPA_GPIO_Port, ROBUS_PTPA_Pin);
        GPIO_InitStruct.Pin = ROBUS_PTPA_Pin;
        HAL_GPIO_Init(ROBUS_PTPA_GPIO_Port, &GPIO_InitStruct);
        HAL_GPIO_WritePin(ROBUS_PTPA_GPIO_Port, ROBUS_PTPA_Pin, GPIO_PIN_SET); // Set the PTPA pin
    }
    else if (branch == BRANCH_B)
    {
        HAL_GPIO_DeInit(ROBUS_PTPB_GPIO_Port, ROBUS_PTPB_Pin);
        GPIO_InitStruct.Pin = ROBUS_PTPB_Pin;
        HAL_GPIO_Init(ROBUS_PTPB_GPIO_Port, &GPIO_InitStruct);
        HAL_GPIO_WritePin(ROBUS_PTPB_GPIO_Port, ROBUS_PTPB_Pin, GPIO_PIN_SET); // Set the PTPB pin
    }
}
/**
 * \fn reverse_detection(void)
 * \brief
 */
void reset_PTP(branch_t branch)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;

    if (branch == BRANCH_A)
    {
        HAL_GPIO_DeInit(ROBUS_PTPA_GPIO_Port, ROBUS_PTPA_Pin);
        __HAL_GPIO_EXTI_CLEAR_IT(ROBUS_PTPB_Pin);
        GPIO_InitStruct.Pin = ROBUS_PTPA_Pin;
        HAL_GPIO_Init(ROBUS_PTPA_GPIO_Port, &GPIO_InitStruct);
    }
    else if (branch == BRANCH_B)
    {
        HAL_GPIO_DeInit(ROBUS_PTPB_GPIO_Port, ROBUS_PTPB_Pin);
        __HAL_GPIO_EXTI_CLEAR_IT(ROBUS_PTPA_Pin);
        GPIO_InitStruct.Pin = ROBUS_PTPB_Pin;
        HAL_GPIO_Init(ROBUS_PTPB_GPIO_Port, &GPIO_InitStruct);
    }
}
/**
 * \fn reverse_detection(void)
 * \brief
 */
unsigned char get_PTP(branch_t branch)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;

    if (branch == BRANCH_A)
    {
        HAL_GPIO_DeInit(ROBUS_PTPA_GPIO_Port, ROBUS_PTPA_Pin);
        GPIO_InitStruct.Pin = ROBUS_PTPA_Pin;
        HAL_GPIO_Init(ROBUS_PTPA_GPIO_Port, &GPIO_InitStruct);
        return (HAL_GPIO_ReadPin(ROBUS_PTPA_GPIO_Port, ROBUS_PTPA_Pin));
    }
    else if (branch == BRANCH_B)
    {
        HAL_GPIO_DeInit(ROBUS_PTPB_GPIO_Port, ROBUS_PTPB_Pin);
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pin = ROBUS_PTPB_Pin;
        HAL_GPIO_Init(ROBUS_PTPB_GPIO_Port, &GPIO_InitStruct);
        return (HAL_GPIO_ReadPin(ROBUS_PTPB_GPIO_Port, ROBUS_PTPB_Pin));
    }
    return 0;
}


/**
 * \fn void hal_init(void)
 * \brief hardware configuration (clock, communication, DMA...)
 */
void hal_init(void)
{
    crc_init();

    // Enable Reception timeout interrupt
    // the timeout expressed in nb of bits duration
    LL_USART_EnableRxTimeout(USART3);
    LL_USART_EnableIT_RTO(USART3);
    LL_USART_SetRxTimeout(USART3, TIMEOUT_VAL * (8 + 1 + 1));

    set_baudrate(DEFAULTBAUDRATE);

    // Setup data direction
    HAL_GPIO_WritePin(ROBUS_DE_GPIO_Port, ROBUS_DE_Pin, GPIO_PIN_RESET); // Disable emitter | Enable Receiver only - Hardware DE impossible
    // Setup pull ups pins
    HAL_GPIO_WritePin(RS485_LVL_UP_GPIO_Port, RS485_LVL_UP_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(RS485_LVL_DOWN_GPIO_Port, RS485_LVL_DOWN_Pin, GPIO_PIN_RESET);

    // Setup PTP lines
    reset_PTP(BRANCH_A);
    reset_PTP(BRANCH_B);
    reset_detection();
}

/**
 * \fn unsigned char hal_transmit(unsigned char* data)
 * \brief write a data byte
 *
 * \param data *data bytes to send
 * \param size size of data to send in byte
 *
 * \return error
 */
unsigned char hal_transmit(unsigned char *data, unsigned short size)
{
    for (unsigned short i = 0; i < size; i++)
    {
        while (!LL_USART_IsActiveFlag_TXE(USART3))
        {
        }
        if (ctx.collision)
        {
            // There is a collision
            ctx.collision = FALSE;
            return 1;
        }
        LL_USART_TransmitData8(USART3, *(data + i));
    }
    return 0;
}

void hal_wait_transmit_end(void)
{
    while (!LL_USART_IsActiveFlag_TC(USART3))
        ;
}
/**
 * \fn void hal_disable_irq(void)
 * \brief disable IRQ
 *
 * \return error
 */
void hal_disable_irq(void)
{
    __disable_irq();
}

/**
 * \fn void hal_enable_irq(void)
 * \brief enable IRQ
 *
 * \return error
 */
void hal_enable_irq(void)
{
    __enable_irq();
}


/**
 * \fn void board_enable_irq(void)
 * \brief enable IRQ
 *
 * \return error
 */
void node_enable_irq(void)
{
    __enable_irq();
}

void node_disable_irq(void)
{
    __disable_irq();
}

uint32_t node_get_systick(void)
{
	return HAL_GetTick();
}

/**
 * \fn void hal_enable_rx(void)
 * \brief enable RX hard channel
 *
 * \return error
 */
void hal_enable_rx(void)
{
    HAL_GPIO_WritePin(ROBUS_RE_GPIO_Port, ROBUS_RE_Pin, GPIO_PIN_RESET);
}

/**
 * \fn void hal_disable_rx(void)
 * \brief disable RX hard channel
 *
 * \return error
 */
void hal_disable_rx(void)
{
    HAL_GPIO_WritePin(ROBUS_RE_GPIO_Port, ROBUS_RE_Pin, GPIO_PIN_SET);
}

/**
 * \fn void hal_enable_tx(void)
 * \brief enable TX hard channel
 *
 * \return error
 */
void hal_enable_tx(void)
{
    HAL_GPIO_WritePin(ROBUS_DE_GPIO_Port, ROBUS_DE_Pin, GPIO_PIN_SET);
    // Sometime the TX set is too slow and the driver switch in sleep mode...
    for (volatile unsigned int i = 0; i < 50; i++)
        ;
}

/**
 * \fn void hal_disable_tx(void)
 * \brief disable TX hard channel
 *
 * \return error
 */
void hal_disable_tx(void)
{
    HAL_GPIO_WritePin(ROBUS_DE_GPIO_Port, ROBUS_DE_Pin, GPIO_PIN_RESET);
}
/******************************************************************************
 * @brief
 *   Luos_HALErasePage: Luos HAL general initialisation
 * @Param
 *
 * @Return
 *
 ******************************************************************************/
void Luos_HALEraseFlashPage(void)
{
	uint32_t page_error = 0;
    FLASH_EraseInitTypeDef s_eraseinit;

    s_eraseinit.TypeErase = FLASH_TYPEERASE_PAGES;
    s_eraseinit.Banks = FLASH_BANK_2;
    s_eraseinit.Page = FLASH_PAGE_NB - 1;
    s_eraseinit.NbPages = 1;

    // Erase Page
    HAL_FLASH_Unlock();
    HAL_FLASHEx_Erase(&s_eraseinit, &page_error);
    HAL_FLASH_Lock();
}
/******************************************************************************
 * @brief
 *   Luos_HALErasePage: Luos HAL general initialisation
 * @Param
 *
 * @Return
 *
 ******************************************************************************/
void Luos_HALWriteFlash(uint32_t addr, uint16_t size, uint8_t *data)
{
    // Before writing we have to erase the entire page
    // to do that we have to backup current falues by copying it into RAM
    uint8_t page_backup[PAGE_SIZE];
    memcpy(page_backup, (void *)ADDRESS_ALIASES_FLASH, PAGE_SIZE);

    // Now we can erase the page
    Luos_HALEraseFlashPage();

    // Then add input data into backuped value on RAM
    uint32_t RAMaddr = (addr - ADDRESS_ALIASES_FLASH);
    memcpy(&page_backup[RAMaddr], data, size);

    // and copy it into flash
    HAL_FLASH_Unlock();

    // ST hal flash program function write data by uint64_t raw data
    for (uint32_t i = 0; i < PAGE_SIZE; i += sizeof(uint64_t))
    {
        while(HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, i + ADDRESS_ALIASES_FLASH, *(uint64_t *)(&page_backup[i])) != HAL_OK);
    }
    HAL_FLASH_Lock();
}
/******************************************************************************
 * @brief
 *   Luos_HALErasePage: Luos HAL general initialisation
 * @Param
 *
 * @Return
 *
 ******************************************************************************/
void Luos_HALReadFlash(uint32_t addr, uint16_t size, uint8_t *data)
{
	memcpy(data, (void *)(addr), size);
}

//******** Alias management ****************
void write_alias(unsigned short local_id, char *alias)
{
	uint32_t addr = ADDRESS_ALIASES_FLASH + (local_id * (MAX_ALIAS_SIZE + 1));
	Luos_HALWriteFlash(addr, 16, (uint8_t*)alias);
}

char read_alias(unsigned short local_id, char *alias)
{
    uint32_t addr = ADDRESS_ALIASES_FLASH + (local_id * (MAX_ALIAS_SIZE + 1));
	Luos_HALReadFlash(addr, 16, (uint8_t*)alias);
	// Check name integrity
	if ((((alias[0] < 'A') | (alias[0] > 'Z')) & ((alias[0] < 'a') | (alias[0] > 'z'))) | (alias[0] == '\0'))
	{
		return 0;
	}
	else
	{
		return 1;
	}
}
