#include <LPC17xx.h>
#include <stdio.h>

// LCD Macros
#define RS_CTRL 0x08000000 // PO.27
#define EN_CTRL 0x10000000 // PO.28
#define DT_CTRL 0x07800000 // PO.23 to P0.26

// Ultrasonic Sensor Pins
#define TRIG_PIN (1 << 4) // P0.4 as Trigger
#define ECHO_PIN (1 << 26) // P1.26 as Echo (must be CAP0.0)

// Buzzer Pin
#define BUZZER_PIN (1 << 5) // P0.5 as Buzzer

// LED Bar Graph Pins
#define LED_PINS 0x007F8000 // P0.15 to P0.22 (8 bits)

// Global Variables
unsigned int motion;
unsigned char flag = 0, lcd_displayed = 0;
unsigned long int temp1 = 0, temp2 = 0, i;
unsigned char flag1 = 0, flag2 = 0;
unsigned char msg[] = {"WELCOME"};
unsigned long int init_command[] = {
    0x30, 0x30, 0x30, 0x20,
    0x28, 0x0C, 0x06, 0x01, 0x80
};

// Globals for Ultrasonic Sensor
volatile uint32_t rising_time = 0;
volatile uint32_t falling_time = 0;
volatile uint32_t pulse_width = 0;
volatile uint32_t distance_cm = 1000;
volatile int new_measurement_ready = 0;

// Counter for WELCOME duration
volatile uint32_t welcome_duration_counter = 0;

// Function Prototypes
void pwm_init(void);
void PWM1_IRQHandler(void);
void delay_ms(unsigned int ms);
void lcd_write(void);
void port_write(void);
void delay_lcd(unsigned int);
void lcd_init(void);
void lcd_clear(void);
void lcd_print(char *str);
void timer_init(void);
void delay_us(unsigned int us);
void send_trigger_pulse(void);
void TIMERO_IRQHandler(void);
void beep_buzzer(void);

int main(void) {
    SystemInit();
    SystemCoreClockUpdate();

    // --- Define new thresholds for hysteresis
    #define NEAR_THRESHOLD 10 // Object is definitely near (cm)
    #define FAR_THRESHOLD 12 // Object is definitely far (cm)

    //--- Sensor Pin Setup
    // P0.4 (TRIG) as Output
    LPC_PINCON->PINSEL0 &= ~(3 << 8);
    LPC_GPIO0->FIODIR |= TRIG_PIN;
    LPC_GPIO0->FIOCLR = TRIG_PIN; // Start with Trig LOW

    // P1.26 (ECHO) as Input (configured in timer_init)

    //--- PO.15-P0.22 (LEDs) as Output ---
    LPC_PINCON->PINSEL0 &= ~(3U << 30); // P0.15 as GPIO
    LPC_PINCON->PINSEL1 &= ~0x00003FFF; // PO.16-P0.22 as GPIO
    LPC_GPIO0->FIODIR |= LED_PINS; // Set all 8 as output
    LPC_GPIO0->FIOCLR = LED_PINS; // Start with all 8 LEDs OFF

    // LCD Pins as Output
    LPC_GPIO0->FIODIR |= DT_CTRL | RS_CTRL | EN_CTRL;

    // P0.5 (BUZZER) as Output
    LPC_PINCON->PINSEL0 &= ~(3 << 10);
    LPC_GPIO0->FIODIR |= BUZZER_PIN;
    LPC_GPIO0->FIOCLR = BUZZER_PIN;

    lcd_init();
    pwm_init();
    timer_init();

    while (1) {
        send_trigger_pulse(); // 1. Send 10us pulse
        delay_ms(60); // 2. Wait 60ms for echo

        if (new_measurement_ready) {
            new_measurement_ready = 0; // Clear the flag
        }

        // 3. Calculate distance
        if (falling_time > rising_time) {
            pulse_width = falling_time - rising_time;
        } else {
            pulse_width = (0xFFFFFFFF - rising_time) + falling_time;
        }
        distance_cm = pulse_width / 58;

        // 4. Use distance to control LED and LCD (with hysteresis)

        //--- STATE 1: Hand is NEAR
        if (distance_cm < NEAR_THRESHOLD && distance_cm > 0) {
            /* --- C89 Declarations must be at the top --- */
            int leds_on = 0;
            uint32_t led_mask = 0;
            int k = 0;
            uint32_t target_brightness;
            /* --- End of Declarations --- */

            welcome_duration_counter++; // Increment counter every loop

            //--- LED Bar Graph Logic ---
            if (distance_cm <= 1) {
                leds_on = 0; // 1cm or less, 0 LEDS
            } else if (distance_cm >= (NEAR_THRESHOLD - 1)) {
                leds_on = 8; // 9cm or more, 8 LEDS
            } else {
                leds_on = distance_cm - 1; // 2cm->1 LED, 3cm->2 LEDs, etc.
            }

            for (k = 0; k < leds_on; k++) {
                led_mask |= (1 << (15 + k));
            }
            LPC_GPIO0->FIOCLR = LED_PINS;
            LPC_GPIO0->FIOSET = led_mask;

            //--- Handle PWM: Smoothly fade to target brightness ---
            // 1. Calculate the target brightness based on distance
            // We map distance [9..1] to brightness [0..29000]
            target_brightness = (9 - distance_cm) * 3625; // (29000/8 = 3625)
            if (target_brightness > 29000) target_brightness = 29000; // Clamp max brightness

            // 2. Compare current brightness to target and move towards it
            if (LPC_PWM1->MR4 < target_brightness) {
                LPC_PWM1->MR4 += 100; // Fade IN step
                if (LPC_PWM1->MR4 > target_brightness) {
                    LPC_PWM1->MR4 = target_brightness; // Don't overshoot
                }
            } else if (LPC_PWM1->MR4 > target_brightness) {
                LPC_PWM1->MR4 -= 100; // Fade OUT step
                if (LPC_PWM1->MR4 > 30000) { // Check for unsigned underflow
                    LPC_PWM1->MR4 = 0;
                }
            }

            if (LPC_PWM1->MR4 < target_brightness) {
                LPC_PWM1->MR4 = target_brightness; // Don't undershoot
            }

            LPC_PWM1->LER = 0xFF; // Load new value
            delay_ms(20); // Keep this delay for a smooth fade
            flag = 0xFF; // Set flag to "on" state (so fade-out in STATE 2 works)

            //--- Handle LCD: Show "WELCOME"
            if (!lcd_displayed) {
                lcd_clear();
                lcd_print("WELCOME");
                lcd_displayed = 1;
                welcome_duration_counter = 0; // Reset counter on first display
            }

            //--- STATE 2: Hand is FAR
        } else if (distance_cm > FAR_THRESHOLD) {
            /* C89 Declarations must be at the top --- */
            char buffer[16];
            uint32_t duration_sec;
            /* --- End of Declarations --- */

            // --- Turn off all 8 LEDs
            LPC_GPIO0->FIOCLR = LED_PINS;

            //--- Handle PWM: Fade OUT (Slower) ---
            if (flag == 0xFF) {
                if (LPC_PWM1->MR4 <= 100) {
                    LPC_PWM1->MR4 = 0;
                    flag = 0x00; // Set state to "off"
                    beep_buzzer(); // Beep exactly once
                } else {
                    LPC_PWM1->MR4 -= 100;
                }
            }
            LPC_PWM1->LER = 0xFF;
            delay_ms(20);

            // --- Handle LCD to show Time and Goodbye
            if (lcd_displayed) {
                // This block runs ONCE when the hand is first removed.
                // CORRECTED Time Calculation
                // (count * 60ms loop) / 1000ms/s ~= count / 16.67
                // After testing, timing was off. (count * 60)/18000
                // Which simplifies to: count / 300
                duration_sec = welcome_duration_counter / 300;
                lcd_clear();
                sprintf(buffer, "Time: %u sec", (unsigned int)duration_sec);
                lcd_print(buffer);
                delay_ms(2000); // Show time for 2 seconds

                lcd_clear();
                lcd_print("GOOD BYE");
                delay_ms(2000); // Show goodbye for 2 seconds

                lcd_clear();
                lcd_displayed = 0; // Reset for the next "WELCOME"
            }

            //--- STATE 3: Hand is in the "dead zone" (10-12 cm)
        } else {
            // Do nothing, but keep the delay for PWM smoothing
            delay_ms(20);
        }
    }
}

// PWM Initialization
void pwm_init(void) {
    LPC_SC->PCONP |= (1 << 6);
    LPC_PINCON->PINSEL3 &= ~(3 << 14);
    LPC_PINCON->PINSEL3 |= (2 << 14);
    LPC_PWM1->PCR = (1 << 12);
    LPC_PWM1->MCR = 0x02;
    LPC_PWM1->MR0 = 30000;
    LPC_PWM1->MR4 = 0;
    LPC_PWM1->LER = 0xFF;
    LPC_PWM1->TCR = 0x02;
    LPC_PWM1->TCR = 0x09;
    NVIC_EnableIRQ(PWM1_IRQn);
}

// Delay in milliseconds (NOTE: This is a blocking, approximate delay)
void delay_ms(unsigned int ms) {
    unsigned int i, j;
    for (i = 0; i < ms; i++)
        for (j = 0; j < 1000; j++);
}

// Microsecond Delay (NOTE: This is a blocking, approximate delay)
void delay_us(unsigned int us) {
    unsigned int i;
    for (i = 0; i < (us * 10); i++);
}

// Buzzer Beep Function
void beep_buzzer(void) {
    LPC_GPIO0->FIOSET = BUZZER_PIN;
    delay_ms(100); // 100ms beep duration
    LPC_GPIO0->FIOCLR = BUZZER_PIN;
}

// PWM Interrupt Service Routine
void PWM1_IRQHandler(void) {
    LPC_PWM1->IR = 0xFF;
    if (flag == 0x00) {
        LPC_PWM1->MR4 += 50;
        LPC_PWM1->LER = 0xFF;
        if (LPC_PWM1->MR4 >= 29000)
            flag = 0xFF;
    } else if (flag == 0xFF) {
        LPC_PWM1->MR4 -= 50;
        LPC_PWM1->LER = 0xFF;
        if (LPC_PWM1->MR4 <= 0)
            flag = 0x00;
    }
}

// Timer/Capture Initialization for Ultrasonic
void timer_init(void) {
    LPC_SC->PCONP |= (1 << 1);
    LPC_PINCON->PINSEL3 |= (3 << 20); // Set P1.26 as CAP0.0
    LPC_TIM0->PR = 24; // (25-1) for 1MHz @ 25MHz PCLK
    LPC_TIM0->CCR = (1 << 0) | (1 << 1) | (1 << 2); // Capture on rising, falling, and interrupt
    NVIC_EnableIRQ(TIMERO_IRQn);
    LPC_TIM0->TCR = 1;
}

// Trigger Pulse Function for Ultrasonic
void send_trigger_pulse(void) {
    LPC_GPIO0->FIOSET = TRIG_PIN;
    delay_us(10);
    LPC_GPIO0->FIOCLR = TRIG_PIN;
}

// Timer0 ISR (Handles Echo)
void TIMERO_IRQHandler(void) {
    static int edge = 0;
    if (LPC_TIM0->IR & (1 << 4)) {
        if (edge == 0) {
            rising_time = LPC_TIM0->CR0;
            edge = 1;
        } else {
            falling_time = LPC_TIM0->CR0;
            new_measurement_ready = 1;
            edge = 0;
        }
    }
    LPC_TIM0->IR = (1 << 4);
}

// LCD Initialization
void lcd_init(void) {
    flag1 = 0;
    for (i = 0; i < 9; i++) {
        temp1 = init_command[i];
        lcd_write();
    }
}

// LCD Print String
void lcd_print(char *str) {
    flag1 = 1;
    i = 0;
    while (str[i] != '\0') {
        temp1 = str[i];
        lcd_write();
        i++;
    }
}

// LCD Write Logic (4-bit mode)
void lcd_write(void) {
    flag2 = (flag1 == 1 ? 0 : ((temp1 == 0x30) || (temp1 == 0x20)) ? 1 : 0);
    temp2 = temp1 & 0xF0;
    temp2 <<= 19;
    port_write();
    if (flag2 == 0) {
        temp2 = temp1 & 0x0F;
        temp2 <<= 23;
        port_write();
    }
}

// Send Data to LCD
void port_write(void) {
    LPC_GPIO0->FIOPIN = temp2;
    if (flag1 == 0)
        LPC_GPIO0->FIOCLR = RS_CTRL;
    else
        LPC_GPIO0->FIOSET = RS_CTRL;
    LPC_GPIO0->FIOSET = EN_CTRL;
    delay_lcd(25);
    LPC_GPIO0->FIOCLR = EN_CTRL;
    delay_lcd(1000000);
}

// Simple Delay for LCD
void delay_lcd(unsigned int r1) {
    unsigned int r;
    for (r = 0; r < r1; r++);
}

// LCD Clear Screen
void lcd_clear(void) {
    flag1 = 0;
    temp1 = 0x01; // Clear display
    lcd_write();
}
