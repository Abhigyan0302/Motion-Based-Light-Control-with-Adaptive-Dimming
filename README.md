# Motion-Based-Light-Control-with-Adaptive-Dimming
In the pursuit of creating smarter and more interactive environments, embedded systems provide
robust solutions for sophisticated, automated control. This project details the development of an
advanced proximity-based welcome system utilizing the LPC1768 microcontroller. The system
integrates an ultrasonic sensor for high-precision distance measurement, a 16x2 character LCD for
dynamic user feedback, an 8-bit LED bar graph for visual proximity indication, a separate PWMcontrolled LED for smooth fade effects, and a buzzer for audible alerts.
The system is designed to measure the distance to an object (e.g., a hand) in real-time. When a user
approaches within a predefined near threshold (10 cm), the system activates: it displays a "WELCOME"
message on the LCD, illuminates a corresponding number of LEDs on the bar graph based on proximity,
and smoothly fades in the PWM LED.
Conversely, when the user moves away (beyond 12 cm), the system initiates a departure sequence. It
fades out the PWM LED, turns off the bar graph, and emits a brief beep from the buzzer. It then
calculates and displays the duration of the user's presence on the LCD, followed by a "GOOD BYE"
message, before returning to its idle state. This project showcases a multi-modal approach to user
interaction, combining distance sensing with layered visual and audible feedback to create an energyefficient and intelligent automated system.
##Components Required
• LPC1768 Microcontroller: The ARM Cortex-M3-based development board, serving as the
system's central controller.
• Ultrasonic Sensor (HC-SR04): Provides distance measurement by emitting an ultrasonic pulse
(Trig) and measuring the time until the echo is received (Echo).
• LCD Display (16x2): A character liquid-crystal display used to show text-based messages like
"WELCOME", "Time: X sec", and "GOOD BYE".
• 8-bit LED Bar Graph: An array of 8 LEDs used to provide a discrete, visual representation of the
user's proximity (e.g., more LEDs light up as the user gets closer).
• Single LED (with resistor): A separate LED controlled by a PWM signal to provide smooth
dimming and brightening effects.
• Buzzer: An active or passive buzzer to provide a single, audible beep during the departure
sequence.
• Connecting Wires and Breadboard: For establishing electrical connections between all
components.
• Power Supply: To provide regulated 5V and 3.3V power to the microcontroller and peripheral
components.
