#include "FreeRTOS.h"
#include "task.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "Pmod_DHB1.h"
#include "PWM.h"


#define DHB1_GPIO_BASEADDR      0x44A00000
#define DHB1_PWM_BASEADDR       0x44A20000

#define IR_SENSOR_GPIO_BASEADDR 0x40020000
#define MAXSONAR_BASEADDR       XPAR_PMOD_DUAL_MAXSONAR_0_BASEADDR

#define CLK_FREQ                XPAR_MICROBLAZE_FREQ

#define PWM_PERIOD_MS           2
#define BASE_SPEED              70

#define IR_LEFT_MASK            0x01
#define IR_RIGHT_MASK           0x02

#define OBSTACLE_DISTANCE_CM    15
#define MAXSONAR_CHANNEL        1

#define TASK_STACK_SIZE         2048



static PmodDHB1 g_motor;
static XGpio g_irSensor;



static void InitializeHardware(void);
static u32 ReadSonarDistance(void);
static void SetMotors(u8 rightSpeed, u8 leftSpeed);
static void vMainTask(void *pvParameters);



// Read distance sonar
static u32 ReadSonarDistance(void)
{
    u32 offset = 4 + ((MAXSONAR_CHANNEL - 1) * 16);
    u32 clkEdges = Xil_In32(MAXSONAR_BASEADDR + offset);
    return (u32)((u64)clkEdges * 10000000ULL / CLK_FREQ / 147 + 5) / 10;
}

//Set  motors
static void SetMotors(u8 rightSpeed, u8 leftSpeed)
{
    DHB1_setMotor1Speed(&g_motor, rightSpeed);
    DHB1_setMotor2Speed(&g_motor, leftSpeed);
}

//Initialize motors and IR GPIOs
static void InitializeHardware(void)
{
    DHB1_begin(&g_motor, DHB1_GPIO_BASEADDR, DHB1_PWM_BASEADDR, CLK_FREQ, PWM_PERIOD_MS);
    DHB1_motorDisable(&g_motor);      
    DHB1_setMotorSpeeds(&g_motor, 0, 0);

    g_irSensor.BaseAddress = IR_SENSOR_GPIO_BASEADDR;
    g_irSensor.IsReady = XIL_COMPONENT_IS_READY;
    g_irSensor.IsDual = 1;
    XGpio_SetDataDirection(&g_irSensor, 1, 0xFFFFFFFF);  // input
}

static void vMainTask(void *pvParameters)
{
    (void)pvParameters;

    u8 irReading, leftIR, rightIR;
    u32 distance;

    // Set direction
    DHB1_setDir1(&g_motor, 1);
    DHB1_setDir2(&g_motor, 1);
    DHB1_motorEnable(&g_motor);

    xil_printf("Robot Started.\r\n");

    while (1)
    {
        // Read IR sensor values
        irReading = XGpio_DiscreteRead(&g_irSensor, 1);
        leftIR = irReading & IR_LEFT_MASK;
        rightIR = irReading & IR_RIGHT_MASK;

        // Read sonar
        distance = ReadSonarDistance();

        xil_printf("L=%d R=%d | Distance=%lu cm\r\n",
                   leftIR ? 1 : 0,
                   rightIR ? 1 : 0,
                   distance);

        if (distance <= OBSTACLE_DISTANCE_CM)
        {
            xil_printf("Obstacle detected. Stopping.\r\n");
            SetMotors(0, 0);
            vTaskDelay(pdMS_TO_TICKS(200));

            xil_printf("Turning right...\r\n");
            SetMotors(60, 0);
            vTaskDelay(pdMS_TO_TICKS(350));

            continue;
        }


        if (leftIR && !rightIR)
        {
            // turn right
            xil_printf("Left IR triggered → turning right\r\n");
            SetMotors(BASE_SPEED, BASE_SPEED / 2);
        }
        else if (!leftIR && rightIR)
        {
            // turn left
            xil_printf("Right IR triggered → turning left\r\n");
            SetMotors(BASE_SPEED / 2, BASE_SPEED);
        }
        else if (leftIR && rightIR)
        {
            //  stop
            xil_printf("Both IR triggered → STOP\r\n");
            SetMotors(0, 0);
        }
        else
        {
            // move forward
            SetMotors(BASE_SPEED, BASE_SPEED);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


int main(void)
{
    xil_printf("Initializing hardware...\r\n");
    InitializeHardware();

    xil_printf("Starting FreeRTOS task...\r\n");
    xTaskCreate(vMainTask, "MainTask", TASK_STACK_SIZE, NULL, 1, NULL);

    vTaskStartScheduler();

    while (1);
    return 0;
}
