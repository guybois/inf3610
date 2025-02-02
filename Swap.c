/*
*********************************************************************************************************
*                                                 uC/OS-III
*                                          The Real-Time Kernel
*                                               PORT Windows
*
*
*            		          					Guy BOIS
*                                  Polytechnique Montreal, Qc, CANADA
*                                                  07/2020
*
* File : Swap.c
*
*********************************************************************************************************
*/

#include  <cpu.h>
#include  <lib_mem.h>

#include <os.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>

#include <app_cfg.h>
#include <cpu.h>
#include <ucos_bsp.h>
#include <ucos_int.h>
#include <xparameters.h>
#include <KAL/kal.h>

#include <xil_printf.h>

#include  <stdio.h>
#include  <ucos_bsp.h>

#include <Source/os.h>
#include <os_cfg_app.h>

#include <xgpio.h>
#include <xintc.h>
#include <xil_exception.h>

XIntc axi_intc;
XGpio gpSwitch;

#define GIC_DEVICE_ID	        XPAR_PS7_SCUGIC_0_DEVICE_ID
#define PL_INTC_IRQ_ID          XPS_IRQ_INT_ID
#define FIT_1S_IRQ_ID           XPAR_AXI_INTC_0_FIT_TIMER_0_INTERRUPT_INTR
#define FIT_3S_IRQ_ID			XPAR_AXI_INTC_0_FIT_TIMER_1_INTERRUPT_INTR
#define GPIO_SW_IRQ_ID			XPAR_AXI_INTC_0_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define GPIO_SW_DEVICE_ID		XPAR_AXI_GPIO_0_DEVICE_ID

#define XGPIO_IR_MASK		0x3 /**< Mask of all bits */


/*
*********************************************************************************************************
*                                              CONSTANTS
*********************************************************************************************************
*/

#define         TASK_STK_SIZE    8192                // Size of each task's stacks (# of WORDs)


#define         TASK_BASSE_PRIO    	10               // Application tasks priorities
#define			TASK_HAUTE_PRIO		8
#define 		All_Tasks  			0x01


/*
*********************************************************************************************************
*                                              VARIABLES
*********************************************************************************************************
*/


CPU_STK          TaskBASSEStk[TASK_STK_SIZE];     	// Startup    task stack
CPU_STK			 TaskHAUTEStk[TASK_STK_SIZE];
CPU_STK 		 StartupTaskStk[UCOS_START_TASK_STACK_SIZE];

OS_TCB			 TaskBASSETCB;
OS_TCB			 TaskHAUTETCB;

OS_TCB 			StartupTaskTCB;

  /* This disables all code until the next "#endif" */

OS_MUTEX		Mutex;
OS_SEM			Sem;								// les semaphores emulent le reveil des tâches

OS_FLAG_GRP 	FlagGroup;

int Temp;											// Mis volontairement pour demontrer la non re entrance


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

void            TaskBASSE(void *data);               // Function prototypes of tasks
void			TaskHAUTE(void *data);

void 			StartupTask (void *p_arg);

/*
*********************************************************************************************************
*                                                  MAIN
*********************************************************************************************************
*/


int main (void)
{

 	OS_ERR  err;

    UCOS_LowLevelInit();

    CPU_Init();
    Mem_Init();
    OSInit(&err);


	OSMutexCreate(&Mutex, "Mutex", &err);
	OSSemCreate(&Sem, "Sem", 0, &err);

    OSTaskCreate(&TaskBASSETCB,"TaskBASSE", TaskBASSE, (void *) 0, TASK_BASSE_PRIO,
       &TaskBASSEStk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);

    OSTaskCreate(&TaskHAUTETCB,"TaskHAUTE", TaskHAUTE, (void *) 0, TASK_HAUTE_PRIO,
       &TaskHAUTEStk[0u], TASK_STK_SIZE/2, TASK_STK_SIZE, 20, 0, (void *) 0,
	   (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);


    OSTaskCreate(&StartupTaskTCB,"Main Task", StartupTask, (void *) 0, UCOS_START_TASK_PRIO,
                 &StartupTaskStk[0], 0, UCOS_START_TASK_STACK_SIZE,
                 0, 0, DEF_NULL, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR), &err);


    OSStart(&err);
    return 0;                                         // Start multitasking
}

/*
*********************************************************************************************************
*                                               TASKS
*********************************************************************************************************
*/


void  TaskBASSE (void *data)
{
    OS_ERR  err;
	CPU_TS ts;

	while(1)
	{

			OSFlagPend(&FlagGroup, All_Tasks, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);

			int x = 1;
			int y = 2;

			OSMutexPend(&Mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
			Temp = x;
			x = y;

			xil_printf("\nTB - J'émule une interruption\n");
			OSSemPost(&Sem, OS_OPT_POST_1, &err);                     // On émule une interruption

			y = Temp;

			xil_printf("\nTB - Je m'apprete a sortir du mutex");

			OSMutexPost(&Mutex, OS_OPT_POST_NO_SCHED, &err);

			xil_printf("\nTB - %d %d\n", x, y);

			OSTimeDly(1000,OS_OPT_TIME_DLY, &err);


	}
}


void TaskHAUTE(void *data)
{

	OS_ERR  err;
	CPU_TS ts;

	while(1)
	{

			OSFlagPend(&FlagGroup, All_Tasks, 0, OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts, &err);

			int x = 3;
			int y = 4;

			xil_printf("\nTH - je pars et j'attends \n");
			OSSemPend(&Sem,0, OS_OPT_PEND_BLOCKING, &ts, &err);   // On émule une interruption
			xil_printf("\nTH - je viens de recevoir le service d'une interruption\n");

			OSMutexPend(&Mutex, 0, OS_OPT_PEND_BLOCKING, &ts, &err);
			Temp = x;
			x = y;
			y = Temp;
			OSMutexPost(&Mutex, OS_OPT_POST_NONE, &err);

		    xil_printf("\nTH - Je viens de sortir du mutex");

			xil_printf("\nTH - %d %d\n", x, y);
			OSTimeDly(1000,OS_OPT_TIME_DLY, &err);

	}
}


/*
*********************************************************************************************************
*                                               STARTUP TASK
*********************************************************************************************************
*/

void StartupTask (void *p_arg)
{
	int i;
	OS_ERR err;
    KAL_ERR kal_err;
    CPU_INT32U tick_rate;
#if (UCOS_START_DEBUG_TRACE == DEF_ENABLED)
    MEM_SEG_INFO seg_info;
    LIB_ERR lib_err;
#endif
#if (APP_OSIII_ENABLED == DEF_ENABLED)
#if (OS_CFG_STAT_TASK_EN == DEF_ENABLED)
    OS_ERR  os_err;
#endif
#endif


    UCOS_IntInit();

#if (APP_OSIII_ENABLED == DEF_ENABLED)
    tick_rate = OS_CFG_TICK_RATE_HZ;
#endif

#if (APP_OSII_ENABLED == DEF_ENABLED)
    tick_rate = OS_TICKS_PER_SEC;
#endif

    UCOS_TmrTickInit(tick_rate);                                /* Configure and enable OS tick interrupt.              */

#if (APP_OSIII_ENABLED == DEF_ENABLED)
#if (OS_CFG_STAT_TASK_EN == DEF_ENABLED)
    OSStatTaskCPUUsageInit(&os_err);
#endif
#endif

    KAL_Init(DEF_NULL, &kal_err);
    UCOS_StdInOutInit();
    UCOS_PrintfInit();


#if (UCOS_START_DEBUG_TRACE == DEF_ENABLED)
    UCOS_Print("UCOS - uC/OS Init Started.\r\n");
    UCOS_Print("UCOS - STDIN/STDOUT Device Initialized.\r\n");
#endif

#if (APP_SHELL_ENABLED == DEF_ENABLED)
    UCOS_Shell_Init();
#endif

#if ((APP_FS_ENABLED == DEF_ENABLED) && (UCOS_CFG_INIT_FS == DEF_ENABLED))
    UCOS_FS_Init();
#endif

#if ((APP_TCPIP_ENABLED == DEF_ENABLED) && (UCOS_CFG_INIT_NET == DEF_ENABLED))
    UCOS_TCPIP_Init();
#endif /* (APP_TCPIP_ENABLED == DEF_ENABLED) */

#if ((APP_USBD_ENABLED == DEF_ENABLED) && (UCOS_CFG_INIT_USBD == DEF_ENABLED) && (UCOS_USB_TYPE == UCOS_USB_TYPE_DEVICE))
    UCOS_USBD_Init();
#endif /* #if (APP_USBD_ENABLED == DEF_ENABLED) */

#if ((APP_USBH_ENABLED == DEF_ENABLED) && (UCOS_CFG_INIT_USBH == DEF_ENABLED) && (UCOS_USB_TYPE == UCOS_USB_TYPE_HOST))
    UCOS_USBH_Init();
#endif /* #if (APP_USBH_ENABLED == DEF_ENABLED) */

#if (UCOS_START_DEBUG_TRACE == DEF_ENABLED)
    Mem_SegRemSizeGet(DEF_NULL, 4, &seg_info, &lib_err);

    UCOS_Printf ("UCOS - UCOS init done\r\n");
    UCOS_Printf ("UCOS - Total configured heap size. %d\r\n", seg_info.TotalSize);
    UCOS_Printf ("UCOS - Total used size after init. %d\r\n", seg_info.UsedSize);

    xil_printf("Frequence du systeme: %d\n", OS_CFG_TICK_RATE_HZ);
#endif

    UCOS_Print("Programme initialise - \r\n");

    OSFlagPost(&FlagGroup, All_Tasks, OS_OPT_POST_FLAG_CLR, &err);

    OSTaskSuspend((OS_TCB *)0,&err);

}

