/**************** (C) COPYRIGHT 2014 �ൺ���˴���ӿƼ����޹�˾ ****************
* �� �� ��: HCBox.C
* �� �� ��: ����
* ��  ��  : KB-6120E �������¶ȿ���
* ����޸�: 2014��4��21��
*********************************** �޶���¼ ***********************************
* ��  ��: V1.1
* �޶���: ����
* ˵  ��: ������ʾ�ӿڣ���ʾ������״̬
*******************************************************************************/

//#include	"BIOS.H"

#include "Pin.H"
#include "BSP.H"

struct	uHCBox
{
	uint8_t		SetMode;		//	�趨�Ŀ��Ʒ�ʽ����ֹ�����ȡ����䡢�Զ� ���ַ�ʽ
	FP32		SetTemp;		//	�趨�Ŀ����¶ȣ�
	FP32		RunTemp;		//	ʵ��������¶ȣ�
	uint16_t	OutValue;		//	�����ź����ֵ[-1000,0,+1000]��������ʾ���ȣ�������ʾ���䡣
}HCBox;

enum
{	//	����������¶ȵķ�ʽ
	MD_Shut,
	MD_Heat,
	MD_Cool,
	MD_Auto
};


/********************************** ����˵�� ***********************************
*	��������ת��
*******************************************************************************/
#define	fanCountListLen	(4u+(1u+2u))
static	uint16_t	fanCountList[fanCountListLen];
static	uint8_t		fanCountList_index = 0;

uint16_t	FanSpeed_fetch( void )
{
	/*	�̶����1s��¼����ת��Ȧ������������
	 *	���μ����������˲��Ľ��������ת�١�
	 */
	uint8_t 	ii, index = fanCountList_index;
	uint16_t	sum = 0u;
	uint16_t	max = 0u;
	uint16_t	min = 0xFFFFu;
	uint16_t	x0, x1, speed;

	x1 = fanCountList[index];
	for ( ii = fanCountListLen - 1u; ii != 0; --ii )
	{
		//	�����������õ��ٶ�
		x0 = x1;
		if ( ++index >= fanCountListLen ){  index = 0u; }
		x1 = fanCountList[index];
		speed = ( x1 - x0 );
		//	�Զ�����ݽ����˲�
		if ( speed > max ) {  max = speed; }
		if ( speed < min ) {  min = speed; }
		sum += speed;
	}

	speed = (uint16_t)( sum - max - min ) / ( fanCountListLen - (1u+2u));
	
	return	speed  * 30u;
}


uint16_t	HCBoxFan_Circle_Read( void )
{
	return	TIM1->CNT;		//��ͣ�ļ���
}



extern  uint16_t	volatile  fan_shut_delay;
void	HCBoxFan_Update( void )
{	//	�������¼ת��Ȧ��
	fanCountList[ fanCountList_index] = HCBoxFan_Circle_Read();
	if ( ++fanCountList_index >= fanCountListLen )
	{
		fanCountList_index = 0u;
	}

	//	���ȿ��ص���̬����
	if ( --fan_shut_delay == 0u )
	{
		HCBoxFan_OutCmd( FALSE );
	}
}



void	set_HCBoxTemp( FP32 TempSet, uint8_t ModeSet )
{
	HCBox.SetTemp = TempSet;
	HCBox.SetMode = ModeSet;
}



FP32	get_HCBoxTemp( void )
{
	return	usRegInputBuf[5] * 0.0625;
}



uint16_t	get_HCBoxOutput( void )
{
	return	HCBox.OutValue;
}



uint16_t	get_HCBoxFanSpeed( void )
{
	return	FanSpeed_fetch();
}

/********************************** ����˵�� ***********************************
*  ������ƣ�����ѭ����ʱ���ܣ�
*******************************************************************************/

void	HCBox_Output( FP32 OutValue )
{
	//	�������״̬
	HCBox.OutValue = OutValue * 1000 + 1000;
	
	if      ( OutValue < 0.0f )
	{
		OutValue = ( uint16_t ) ( - OutValue * 1000 );
		//	�رռ���
		HCBoxHeat_OutCmd( 0 );
		//	��������
		if      ( OutValue > 990 )
		{
			HCBoxCool_OutCmd( 1000 );
			//	delay( 1000u );
		}
		else if ( OutValue < 10  )
		{
			HCBoxCool_OutCmd( 0 );
			//	delay( 1000u );
		}
		else
		{	
			HCBoxCool_OutCmd( OutValue );
		}
	}
	else if ( OutValue > 0.0f )
	{
		OutValue = ( uint16_t )  OutValue * 1000;
		//	�ر�����
		HCBoxCool_OutCmd( 0 );
		//	��������
		if      ( OutValue >  990 )
		{
			
			HCBoxHeat_OutCmd( 1000 );
			//	delay( 1000u );
		}
		else if ( OutValue < 10 )
		{
			HCBoxHeat_OutCmd( 0 );
			//	delay( 1000u );
		}
		else
		{
			HCBoxHeat_OutCmd( OutValue );
		}
	}
	else
	{
		//	�رռ���
		HCBoxHeat_OutCmd( 0 );
		//	�ر�����
		HCBoxCool_OutCmd( 0 );

		//	delay( 1000u );
	}

	HCBoxFan_Update();			//	��������ת��
}


/********************************** ����˵�� ***********************************
*	�����������乲��һ���¶��źţ����߲���ͬʱʹ�á�
*******************************************************************************/
extern	uint16_t	iRetry;
uint16_t HCBoxOutValue;
void	HCBoxTemp_Update( void )
{
	if ( iRetry >= 30 )
	{
		HCBox_Output( 0.0f );	//	ע����ȴ�״̬���������һ��
	}
	HCBox.RunTemp = get_HCBoxTemp();
	set_HCBoxTemp( usRegHoldingBuf[5] * 0.0625f, usRegHoldingBuf[6] );	
}



/********************************** ����˵�� ***********************************
*  �ȴ�״̬����ֹ���ȡ�����
*******************************************************************************/
volatile	BOOL	EN_Cool = TRUE;
volatile	BOOL	EN_Heat = TRUE;
static	void	HCBox_Wait( void )
{
	static uint16_t Shut_DelayHeat = 0;
	static uint16_t Shut_DelayCool = 0;
	//	�����Զ�ģʽ���޷�ȷ��ʵ�ʹ���ģʽ����ʱ����ȴ�״̬
	if ( MD_Shut == HCBox.SetMode )
	{
		set_HCBoxTemp( usRegHoldingBuf[5] * 0.0625f, usRegHoldingBuf[6] );	
	}
	HCBox_Output( 0.0f );	//	�ȴ�״̬���
	HCBoxTemp_Update();
	if( MD_Auto == HCBox.SetMode )
	{
		if( ( EN_Heat == FALSE ) && ( EN_Cool == FALSE ) )		
		{
			if( HCBox.RunTemp > HCBox.SetTemp )
			{
				EN_Heat = FALSE; 
				EN_Cool = TRUE;
			}
			else
			{
				EN_Heat = TRUE;
				EN_Cool = FALSE;
			}
		}					
		if( ( ( HCBox.RunTemp - HCBox.SetTemp ) >=  2 ) && ( EN_Heat == TRUE ) )
			Shut_DelayHeat ++;
		if( ( HCBox.RunTemp - HCBox.SetTemp ) <   2 )
			Shut_DelayHeat = 0;
		if( ( ( HCBox.RunTemp - HCBox.SetTemp ) <= -2 ) && ( EN_Cool == TRUE ) )
			Shut_DelayCool ++;
		if( ( HCBox.RunTemp - HCBox.SetTemp ) >  -2 )
			Shut_DelayCool = 0;
		if( Shut_DelayHeat >= 60 * 1 )
		{
			Shut_DelayHeat = 
			Shut_DelayCool = 0;
			EN_Heat = FALSE;
			EN_Cool = TRUE;
		}
		else if( EN_Heat == FALSE )
			Shut_DelayHeat = 0;	
		if ( Shut_DelayCool >= 60 * 1 )
		{
			Shut_DelayHeat = 
			Shut_DelayCool = 0;
			EN_Heat = TRUE;
			EN_Cool = FALSE;
		}
		else if( EN_Cool == FALSE )
			Shut_DelayCool = 0;
	}
}




/********************************** ����˵�� ***********************************
*  ����״̬�����䷽ʽ����
*******************************************************************************/

static	void	HCBox_Cool( void )
{
	const	FP32	Kp = 0.1171875f;    //  15/128
	const	FP32	Ki = ( Kp / 240.0f );
	const	FP32	Kd = ( Kp *  80.0f );

	FP32	 TempRun, TempSet;
	static FP32	Ek_1, Ek = 0.0f;
	static FP32	Up = 0.0f, Ui = 0.0f, Ud = 0.0f;
	static FP32	Upid = 0.0f;

    HCBoxTemp_Update();		//	ʵʱ��ȡ�¶�;  if ( ʧ�� ) ת�����״̬
    
    //	����PID����������ֵ��һ����[-1.0�� 0.0]��Χ
    TempRun = HCBox.RunTemp;
    TempSet = HCBox.SetTemp;
    Ek_1 = Ek;
    Ek = ( TempSet - TempRun );

	if( EN_Cool )
	{
		HCBoxTemp_Update();		//	ʵʱ��ȡ�¶�;  if ( ʧ�� ) ת�����״̬
		//	����PID����������ֵ��һ����[-1.0�� 0.0]��Χ
		TempRun = HCBox.RunTemp;
		TempSet = HCBox.SetTemp;
		Ek_1 = Ek;
		Ek = ( TempSet - TempRun );
		Up = Kp * Ek;
		Ui += Ki * Ek;
		if ( Ui < -0.50f ){  Ui = -0.50f; }
		if ( Ui > +0.50f ){  Ui = +0.50f; }
		Ud = ( Ud * 0.8f ) + (( Kd * ( Ek - Ek_1 )) * 0.2f );
		Upid = Up + Ui + Ud;;
		if ( Upid >  0.0f ){  Upid =  0.0f; }
		if ( Upid < -1.0f ){  Upid = -1.0f; }

		//	����������ƣ����䷽ʽ�¿������ȣ��ݲ����٣�2014��1��15�գ�
		if ( Upid < 0.0f )
		{
			fan_shut_delay = 60u;
			HCBoxFan_OutCmd( TRUE );
		}
		
		//	���
        if ( FanSpeed_fetch() < 100u )
        {	//	���Ȳ�ת����ֹ����Ƭ����
            HCBox_Output( 0.0f );	//	ע����ȴ�״̬���������һ��
        }
        else
		{
			HCBox_Output( Upid );	//	����״̬���������ѭ����ʱ���ܣ�
		}

		switch ( HCBox.SetMode )
		{
		case MD_Auto:
// 			//	����¶�ƫ���2'C��ά��һ��ʱ�䣨30min��, �л�������ʽ
// 			if ( Ek > -1.5f )
// 			{
// 				shutcount_Cool = 0u;
// 			}
// 			else if ( shutcount_Cool < ( 60u * 1 ))//30u
// 			{
// 				++shutcount_Cool;
// 			}
// 			else
// 			{
// 				EN_Cool = FALSE;
// 				EN_Heat = TRUE;
// 			}
			break;
		case MD_Cool:	EN_Cool = TRUE; 	break;
		case MD_Heat:	EN_Cool = FALSE;	break;
		default:
		case MD_Shut:	EN_Cool = FALSE;	break;
		}

	}
}

/********************************** ����˵�� ***********************************
*  ����״̬�����ȷ�ʽ����
*******************************************************************************/

static	void	HCBox_Heat( void )  //  5/128
{
	const	FP32	Kp = 0.0390625f;
	const	FP32	Ki = ( Kp / 160.0f );
	const	FP32	Kd = ( Kp *  80.0f );

//	const	FP32	Kp = 0.2;
//	const	FP32	Ki = ( Kp / 100.0f );
//	const	FP32	Kd = ( Kp *  10.0f );

	FP32	TempRun, TempSet;
	static FP32	Ek_1, Ek = 0.0f;
	static FP32	Up = 0.0f, Ui = 0.0f, Ud = 0.0f;
	static FP32	Upid = 0.0f;

	if( EN_Heat )
	{
		HCBoxTemp_Update();
		//	����PID����������ֵ��һ����[0.0 ��+1.0]��Χ
		TempRun = HCBox.RunTemp;
		TempSet = HCBox.SetTemp;
		Ek_1 = Ek;
		Ek = ( TempSet - TempRun );
		Up = Kp * Ek;
		Ui += Ki * Ek;
		if ( Ui < -0.25f ){  Ui = -0.25f; }
		if ( Ui > +0.25f ){  Ui = +0.25f; }
		Ud = ( Ud * 0.8f ) + (( Kd * ( Ek - Ek_1 )) * 0.2f );
		Upid = ( Up + Ui + Ud );
		if ( Upid <  0.0f ){  Upid = 0.0f; }
		if ( Upid > +1.0f ){  Upid = 1.0f; }

		HCBox_Output( Upid );	//	����״̬���������ѭ����ʱ���ܣ�

		switch ( HCBox.SetMode )
		{
		case MD_Auto:
// 			//	����¶�ƫ���2'C��ά��һ��ʱ�䣨30min��, �л�������ʽ
// 			if ( Ek < +1.5f )
// 			{
// 				shutcount_Heat = 0u;
// 			}
// 			else if ( shutcount_Heat < ( 60u * 1u ))//30
// 			{
// 				++shutcount_Heat;
// 			}
// 			else
// 			{
// 				EN_Heat = FALSE;
// 				EN_Cool = TRUE;
// 			}
			break;
		case MD_Heat:	EN_Heat = TRUE; 	break;
		case MD_Cool:	EN_Heat = FALSE;	break;
		default:
		case MD_Shut:	EN_Heat = FALSE;	break;
		}
	}
}


/********************************** ����˵�� ***********************************
*	�������¶ȿ���
*******************************************************************************/
void	HCBoxControl( void )
{
    
	if( HCBoxFlag )
	{
		HCBoxFlag = FALSE;
		HCBox_Wait();	//	dummy read, skip 0x0550
		switch ( HCBox.SetMode )
		{
		case MD_Auto:
			if      (( HCBox.SetTemp + 1 < HCBox.RunTemp ) && EN_Cool )
			{
				HCBox_Cool();
			}
			else if (( HCBox.SetTemp - 1 > HCBox.RunTemp ) && EN_Heat )
			{
				HCBox_Heat();
			}
// 			else
// 			{	//	�����Զ�ģʽ���޷�ȷ��ʵ�ʹ���ģʽ����ʱ����ȴ�״̬
// 				HCBox_Wait();
// 			}
			break;
		case MD_Cool:	EN_Cool = TRUE; EN_Heat = FALSE;	HCBox_Cool();	break;
		case MD_Heat:	EN_Heat = TRUE; EN_Cool = FALSE;	HCBox_Heat();	break;
		default:
		case MD_Shut:	EN_Heat = EN_Cool = FALSE;				HCBox_Wait();	break;
									
		}
	}
		
}








/********  (C) COPYRIGHT 2014 �ൺ���˴���ӿƼ����޹�˾  **** End Of File ****/













































#if 0

// ʹ��TIMx��CH1�Ĳ����ܣ���DMA��¼���������.
#define	CMR_Len	10
static	uint16_t	CMRA[CMR_Len];

// void	CMR1( void )
// {
// 	DMA_Channel_TypeDef	* DMA1_Channelx = DMA1_Channel6;
// 	TIM_TypeDef * TIMx = TIM16;
// 	
// 	//	DMA1 channel1 configuration
// 	SET_BIT ( RCC->AHBENR,  RCC_AHBENR_DMA1EN );
// 	//	DMAģ�����, ��������
// 	DMA1_Channelx->CCR  = 0u;
// 	DMA1_Channelx->CCR  = DMA_CCR6_PL_0						//	ͨ�����ȼ���01 �е�
// 						| DMA_CCR6_PSIZE_0					//	�ڴ�����λ��01 16λ
// 						| DMA_CCR6_MSIZE_0					//	��������λ��01 16λ
// 						| DMA_CCR6_MINC						//	����ģʽ���ڴ�����
// 						| DMA_CCR6_CIRC						//	ѭ�����䣺ʹ��ѭ��
// 					//	| DMA_CCR6_DIR						//	���ͷ��򣺴������
// 						;
// 	DMA1_Channelx->CPAR  = (uint32_t) &TIM16->CCR1;			//	����DMA�����ַ
// 	DMA1_Channelx->CMAR  = (uint32_t) CMRA;					//	�ڴ��ַ
// 	DMA1_Channelx->CNDTR = CMR_Len;							//	��������
// 	SET_BIT ( DMA1_Channelx->CCR, DMA_CCR1_EN );			//	ʹ��DMAͨ��

// 	//	����TIMx �������벶��
// 	SET_BIT( RCC->APB2ENR, RCC_APB2ENR_TIM16EN );
// 	TIMx->CR1   = 0u;
// 	TIMx->CR2   = 0u;
// 	TIMx->CCER  = 0u;
// 	TIMx->CCMR1 = 0u;
// 	TIMx->CCMR2 = 0u;
// 	//	TIMx ʱ����ʼ��: ����ʱ��Ƶ��24MHz����Ƶ��1MHz�����롣
// 	//	ʱ���������Բ���������ٶ�������ٶȡ�
// 	TIMx->PSC = 240u - 1;	//	10us @ 24MHz
// 	TIMx->ARR = 0xFFFFu;
// 	TIMx->EGR = TIM_EGR_UG;
// 	
// 	TIMx->CCMR1 = TIM_CCMR1_CC1S_0					//	CC1S  : 01b   IC1 ӳ�䵽IT1�ϡ�
// 				| TIM_CCMR1_IC1F_1|TIM_CCMR1_IC1F_0	//	IC1F  : 0011b ���������˲�����8����ʱ��ʱ�������˲�
// 				| TIM_CCMR1_IC2PSC_1				//	IC1PSC: 01b   ���������Ƶ��ÿ��2���¼�����һ�β���
// 				;
// 	TIMx->CCER  = TIM_CCER_CC1E						//	���� CCR1 ִ�в���
// 				| TIM_CCER_CC1P						//	������CCR1�����ź����ڡ�
// 				;
// 	TIMx->DIER  = TIM_DIER_CC1DE;

// 	TIMx->CR1   = TIM_CR1_CEN;						//	ʹ�ܶ�ʱ��

// 	//	���ùܽţ�PA.6 ��������
// 	SET_BIT( RCC->APB2ENR, RCC_APB2ENR_IOPAEN );
// 	MODIFY_REG( GPIOA->CRL, 0x0F000000u, 0x04000000u );
// }

uint16_t	fetchSpeed( void )
{	//	ȡ DMA ���� �� �ڴ��ַָ�룬��������ǰ�������Ρ�
	//	���DMA���� �� �ڴ�ָ�붼�����ã�ȡN�εĲ�ֵ�����������ֵ����Сֵ��
	
	/*	�̶����1s��¼����ת��Ȧ������������
	 *	���μ����������˲��Ľ��������ת�١�
	 */
	DMA_Channel_TypeDef	* DMA1_Channelx = DMA1_Channel6;
	uint8_t 	ii, index;
	uint16_t	sum = 0u;
//	uint16_t	max = 0u;
//	uint16_t	min = 0xFFFFu;
	uint16_t	x0, x1, period;

	index = ( DMA1_Channelx->CMAR - ( uint32_t ) CMRA ) / sizeof( uint16_t);	//	�ڴ��ַ
	if ( ++index >= CMR_Len ){  index = 0u; }
	if ( ++index >= CMR_Len ){  index = 0u; }
	
	x1 = CMRA[index];
	for ( ii = CMR_Len - 2u; ii != 0; --ii )
	{
		//	�����������õ��ٶ�
		x0 = x1;
		if ( ++index >= CMR_Len ){  index = 0u; }
		x1 = CMRA[index];
		period = (uint16_t)( x1 - x0 );
		//	�Զ�����ݽ����˲�
//		if ( period > max ) {  max = period; }
//		if ( period < min ) {  min = period; }
//		sum += period;
	}
	period = sum / ( CMR_Len - 2u );
//	period = (uint16_t)( sum - max - min ) / ( CMR_Len - (1u+2u));

	if ( period == 0u )
	{
		return	0xFFFFu;
	}
	else
	{	//	ÿ���ӵļ������� / ÿ������ļ���ʱ�� => ÿ���ӵ�ת��
		return	(( 60u * 100000u ) / period );
	}
}

#endif
// iCount;


// _isr_t( void )

// {
// 	static uint8_t iPWM =0;
// 	iCount ++;
// 	if ( (t1 - t0) > 1000 )
// 	//	t0 += 1000;
// 	(t0 = t1;
// 	{
// 		
// 	}
// 	
// 	
// 	++iPWM;
// 	if (iPWM>= 100 )
// 	{
// 		iPWM = 0;
// 	}
// 	
// 	Heat = iPWM>out
// 	
// }
// void heat( void )
// {
// 	static t0;// = iCount;
// 	t1 = iCount;
// 	if ( (t1 - t0) > 1000 )
// 	//	t0 += 1000;
// 	(t0 = t1;
// 		
// 	{
// 		pid = ???
// 		out = pid;		
// 	}
// 	
// 	
// }
