#include "robot.hpp"

// ** DON'T INCLUDE <iostream>! THINGS WILL BREAK! **
#include <cstdarg>
#include <ctime>
#include <string>
#include <array>

#include <logger.hpp>

#include "commands.hpp"
#include "io-expander.hpp"
#include "TaskSignals.hpp"
#include "neostrip.hpp"

#include "buzzer.hpp"

const std::string filename = "rj-fpga.nib";
const std::string filesystemname = "local";
const std::string filepath = "/" + filesystemname + "/" + filename;

// ADCDMA adc;
// DMA dma;


/**
 * @brief      { Sets the hardware configurations for the status LEDs & places into the given state }
 *
 * @param[in]  state  { the next state of the LEDs }
 */
void statusLights(bool state)
{
	DigitalInOut init_leds[] = {
		{RJ_BALL_LED, PIN_OUTPUT, OpenDrain, !state},
		{RJ_RX_LED,   PIN_OUTPUT, OpenDrain, !state},
		{RJ_TX_LED,   PIN_OUTPUT, OpenDrain, !state},
		{RJ_RDY_LED,  PIN_OUTPUT, OpenDrain, !state}
	};

	for (int i = 0; i < 4; i++)
		init_leds[i].mode(PullUp);
}

/**
 * @brief      { Turns all status LEDs on }
 *
 * @param      args  { nothing }
 */
void statusLightsON(void const* args)
{
	statusLights(1);
}

/**
 * @brief      { Turns all status LEDs on }
 *
 * @param      args  { nothing }
 */
void statusLightsOFF(void const* args)
{
	statusLights(0);
}

// // used to output next analog sample whenever a timer interrupt occurs
// void Sample_timer_interrupt(void const* args)
// {
// 	// send next analog sample out to D to A
// 	// buzzer = Analog_out_data[j];
// 	// increment pointer and wrap around back to 0 at 128
// 	j = (j + 1) & 0x07F;
// }

// void sampleInputs(void)
// {
// 	// Set global variables here for the config input values from the IO Expander.
// 	// This is where the robot's ID comes from, so it's pretty important.
// 	LOG(SEVERE, "Interrupt triggered!");
// }


extern "C" void HardFault_Handler(void)
{
	__asm volatile
	(
	    " tst lr, #4                                                \n"
	    " ite eq                                                    \n"
	    " mrseq r0, msp                                             \n"
	    " mrsne r0, psp                                             \n"
	    " ldr r1, [r0, #24]                                         \n"
	    " ldr r2, hard_fault_handler_2_const                        \n"
	    " bx r2                                                     \n"
	    " hard_fault_handler_2_const: .word HARD_FAULT_HANDLER    	\n"
	);
}

extern "C" void HARD_FAULT_HANDLER(uint32_t* stackAddr)
{
	/* These are volatile to try and prevent the compiler/linker optimising them
	away as the variables never actually get used.  If the debugger won't show the
	values of the variables, make them global my moving their declaration outside
	of this function. */
	volatile uint32_t r0;
	volatile uint32_t r1;
	volatile uint32_t r2;
	volatile uint32_t r3;
	volatile uint32_t r12;
	volatile uint32_t lr; /* Link register. */
	volatile uint32_t pc; /* Program counter. */
	volatile uint32_t psr;/* Program status register. */

	r0 = stackAddr[0];
	r1 = stackAddr[1];
	r2 = stackAddr[2];
	r3 = stackAddr[3];
	r12 = stackAddr[4];
	lr = stackAddr[5];
	pc = stackAddr[6];
	psr = stackAddr[7];

	LOG(FATAL,
	    "\r\n"
	    "================================\r\n"
	    "========== HARD FAULT ==========\r\n"
	    "\r\n"
	    "  MSP:\t0x%08X\r\n"
	    "  HFSR:\t0x%08X\r\n"
	    "  CFSR:\t0x%08X\r\n"
	    "\r\n"
	    "  r0:\t0x%08X\r\n"
	    "  r1:\t0x%08X\r\n"
	    "  r2:\t0x%08X\r\n"
	    "  r3:\t0x%08X\r\n"
	    "  r12:\t0x%08X\r\n"
	    "  lr:\t0x%08X\r\n"
	    "  pc:\t0x%08X\r\n"
	    "  psr:\t0x%08X\r\n"
	    "\r\n"
	    "========== HARD FAULT ==========\r\n"
	    "================================",
	    __get_MSP,
	    SCB->HFSR,
	    SCB->CFSR,
	    r0, r1, r2, r3, r12,
	    lr, pc, psr
	   );

	// do nothing so everything remains unchanged for debugging
	while (true) {};
}


extern "C" void NMI_Handler()
{
	printf("NMI Fault!\n");
	//NVIC_SystemReset();
}
extern "C" void MemManage_Handler()
{
	printf("MemManage Fault!\n");
	//NVIC_SystemReset();
}
extern "C" void BusFault_Handler()
{
	printf("BusFault Fault!\n");
	//NVIC_SystemReset();
}
extern "C" void UsageFault_Handler()
{
	printf("UsageFault Fault!\n");
	//NVIC_SystemReset();
}


/**
 * [main Main The entry point of the system where each submodule's thread is started.]
 * @return  [none]
 */
int main(void)
{
	// NeoStrip rgbLED(RJ_NEOPIXEL, 1);
	// rgbLED.setBrightness(0.2);
	// rgbLED.setPixel(0, 0x00, 0xFF, 0x00);
	// rgbLED.write();

	// Turn on some startup LEDs to show they're working, they are turned off before we hit the while loop
	statusLightsON(nullptr);

	// Set the default logging configurations
	isLogging = RJ_LOGGING_EN;
	rjLogLevel = INIT;

	/* Always send out an empty line at startup for keeping the console
	 * clean on after a 'reboot' command is called;
	 */
	if (isLogging) {
		std::printf("\r\n");
		fflush(stdout);
	}

	/* Set the system time to the most recently known time. The compilation
	 * time is used here. The timestamp should always be the same when using GCC.
	 */
	// time_t sys_time = time(nullptr);

	// // Only set the system time if we're powering up and were off before
	// if (localtime(&sys_time)) {
	// 	const std::string sysDate = __DATE__;
	// 	const std::string sysTime = __TIME__;
	// 	std::tm tt;

	// 	tt.tm_mon  = atoi(sysDate.substr(0, sysDate.find(' ')).c_str());
	// 	tt.tm_mday = atoi(sysDate.substr(tt.tm_mon, sysDate.find_first_not_of(' ') - 1).c_str());
	// 	tt.tm_year = atoi(sysDate.substr(tt.tm_mday).c_str());

	// 	tt.tm_hour = atoi(sysTime.substr(0, sysTime.find(';')).c_str());
	// 	tt.tm_min  = atoi(sysTime.substr(tt.tm_hour, sysTime.find(';')).c_str());
	// 	tt.tm_sec  = atoi(sysTime.substr(tt.tm_min, sysTime.find(' ')).c_str());
	// 	tt.tm_year = 1900 - atoi(sysTime.substr(tt.tm_sec).c_str());

	// 	set_time(mktime(&tt));

	// 	//LOG(INIT, "Build%s", sysData.c_str(), sysTime.c_str());

	// 	LOG(INIT,
	// 	    "System time set to:\t%02u/%02u/%04u %02u:%02u:%02u",
	// 	    tt.tm_mon, tt.tm_mday, tt.tm_year,
	// 	    tt.tm_hour, tt.tm_min, tt.tm_sec
	// 	   );
	// }


	// Setup the interrupt priorities before launching each subsystem's task thread.
	setISRPriorities();

	// Start a periodic blinking LED to show system activity
	DigitalOut ledOne(LED1, 0);
	RtosTimer live_light(imAlive, osTimerPeriodic, (void*)&ledOne);
	live_light.start(RJ_LIFELIGHT_TIMEOUT_MS);

	// TODO: write a function that will recalibrate the radio for this.
	// Reset the ticker on every received packet. For now, we just blink an LED.
	// DigitalOut led4(LED4, 0);
	// RtosTimer radio_timeout_task(imAlive, osTimerPeriodic, (void*)&led4);
	// radio_timeout_task.start(300);

	// Flip off the startup LEDs after a timeout period
	RtosTimer init_leds_off(statusLightsOFF, osTimerOnce);
	init_leds_off.start(RJ_STARTUP_LED_TIMEOUT_MS);

#if RJ_FPGA_ENABLE
	// Create an object for communicating with the FPGA
	FPGA fpga(
	    RJ_SPI_BUS,
	    RJ_FPGA_nCS,
	    RJ_FPGA_PROG_B,
	    RJ_FPGA_INIT_B,
	    RJ_FPGA_DONE
	);

	// This is where the FPGA is actually configured with the bitfile's name passed in
	bool fpga_ready = fpga.Init(filepath);

	if (fpga_ready == true) {

		LOG(INIT, "FPGA Configuration Successful!");
	} else {

		LOG(FATAL, "FPGA Configuration Failed!");
	}

#endif

	// // Setup the IO Expander's hardware
	// MCP23017::Init();

	// // Setup some extended LEDs and turn them on
	// IOExpanderDigitalOut led_err_m1(IOExpanderPinB0);
	// led_err_m1 = 1;

	// uint8_t robot_id = MCP23017::digitalWordRead() & 0x0F;
	// LOG(INIT, "Robot ID:\t%u", robot_id);

	motors_Init();

	// Start the thread task for the on-board control loop
	Thread controller_task(Task_Controller, nullptr, osPriorityHigh);

	// Start the thread task for handling radio communications
	Thread comm_task(Task_CommCtrl, nullptr, osPriorityAboveNormal);

	// Start the thread task for the serial console
	Thread console_task(Task_SerialConsole, nullptr, osPriorityBelowNormal);

	// Attach an interrupt callback for setting the buttons/switches states
	// into the firmware anytime one of them changes
	
	// InterruptIn configInputs(RJ_IOEXP_INT);
	// configInputs.rise(&sampleInputs);


#if RJ_WATCHDOG_TIMER_EN
	// Enable the watchdog timer if it's set in configurations.
	Watchdog::Set(RJ_WATCHDOG_TIMER_VALUE);
#endif

	/* This needs some work. Probably best to just drop DMA for the ADC and
	 * configure the 3 ADC channels at startup to be in burst mode at a very
	 * low rate. Then the ADC registers should always have a valid reading.
	 */

	/*
	adc.SetChannels({ RJ_BALL_DETECTOR, RJ_BATT_SENSE, RJ_5V_SENSE });

	if (!adc.Start()) {
		DigitalOut led3(LED3, 1);

		// error
	}

	if (!dma.Init()) {	// currently hard coded to always return false
		DigitalOut led2(LED4, 1);

		// error
	}

	adc.BurstRead();
	*/

	// Buzzer buzz;
	// buzz.play(969.0, 500, 0.6);
	// Thread::wait(50);
	// buzz.play(800.0, 500, 0.8);
	// Thread::wait(50);
	// buzz.play(920.0, 500, 1.0);
	//buzz.play(0, 100, 0);

	DigitalOut rdy_led(RJ_RDY_LED, !fpga_ready);

	if (fpga_ready) {
		// NeoStrip rgbLED(RJ_NEOPIXEL, 1);
		// rgbLED.setBrightness(1.0);
		// rgbLED.setPixel(0, 0x00, 0xFF, 0x00);
		// rgbLED.write();
	}

	std::array<uint16_t, 5> duty_cycles_r = { 0 };
	std::array<uint16_t, 5> duty_cycles_w = { 2, 7, 3, 15, 120 };
	std::array<uint16_t, 5> enc_deltas = { 0 };
	uint8_t status_byte, j = 0;
	bool motor_on = false;

	LOG(INIT, "FPGA git commit hash:\t0x%08X", fpga.git_hash());

	while (true) {

		rdy_led = !fpga_ready;
		Thread::wait(1000);	// Ping back to main every 1 second seems to perform better than calling Thread::yeild() for some reason?

		for (size_t i = 0; i < duty_cycles_w.size(); i++)
			duty_cycles_w[i] += 50;

		status_byte = fpga.read_duty_cycles(duty_cycles_r.data(), duty_cycles_r.size());

		LOG(OK,
		    "DUTY CYCLES\t(read):\r\n"
		    "    (0x%02X)\t0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X",
		    status_byte, duty_cycles_r.at(0), duty_cycles_r.at(1),
		    duty_cycles_r.at(2), duty_cycles_r.at(3), duty_cycles_r.at(4)
		   );

		status_byte = fpga.set_duty_get_enc(duty_cycles_w.data(), duty_cycles_w.size(),
		                                    enc_deltas.data(), enc_deltas.size());

		LOG(OK,
		    "ENC DELTAS\t(duty cycle write to values on 2nd lines):\r\n"
		    "    (0x%02X)\t0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X\r\n"
		    "            \t0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X",
		    status_byte,
		    enc_deltas.at(0), 	enc_deltas.at(1), 	 enc_deltas.at(2),    enc_deltas.at(3),    enc_deltas.at(4),
		    duty_cycles_w.at(0), duty_cycles_w.at(1), duty_cycles_w.at(2), duty_cycles_w.at(3), duty_cycles_w.at(4)
		   );

		if (j > 2) {
			fpga.motors_en(motor_on);
			motor_on = !motor_on;
			j = 0;
		}

		j++;
	}
}


// The below commented code are things that I worked towards but never brought to a functional state

/*
// Power up timer 0
LPC_SC->PCONP |= (1 << 1);

// Set divider to CLK/1
LPC_SC->PCLK0 |= (0x01 << 2);

// LPC_PINCON->PINSEL4 |= (0x00);
//
PINCON->PINMODE4 |= (0x03);

// select timer pins in PINSEL reg
//select pin modes in PINMODE
// Interrupt set enable register for interrupt

NVIC_SetVector(TIMER0_IRQn, (uint32_t)TIMER0_IRQHandler);
NVIC_EnableIRQ(TIMER0_IRQn);
*/

/*
// Start the flash signature generation
*(long unsigned int*)0x40084024 |= (0x01 << 17) | ((LPC_RAM_BASE / 16) << 16);

// Wait for the flash signature to be generated
while ( !((*(long unsigned int*)0x40084FE0) & (0x01 << 2)) ) {};

LOG(OK,
    "Flash Signature: 0x%08X-%08X-%08X-%08X",
    *(long unsigned int*)0x4008402C,
    *(long unsigned int*)0x40084030,
    *(long unsigned int*)0x40084034,
    *(long unsigned int*)0x40084038
   );
*/
