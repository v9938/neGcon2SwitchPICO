#include "PsxNewLib.h"
#include <SPI.h>

/** \brief Attention Delay
 *
 * Time between attention being issued to the controller and the first clock
 * edge (us).
 */
const byte ATTN_DELAY = 50;

// Set up the speed, data order and data mode
//static SPISettings spiSettings (250000, LSBFIRST, SPI_MODE3);

template <unsigned int PIN_ATT>

class PsxControllerPICO_HwSpi: public PsxController {
private:
//	DigitalPin<PIN_ATT> att;
//	DigitalPin<MOSI> cmd;
//	DigitalPin<MISO> dat;
//	DigitalPin<SCK> clk;

protected:
	virtual void attention () override {
		gpio_put(PIN_ATT, LOW);
//		SPI.beginTransaction (spiSettings);

		delayMicroseconds (ATTN_DELAY);
	}
	
	virtual void noAttention () override {
		//~ delayMicroseconds (5);
		
		SPI.endTransaction ();

		// Make sure CMD and CLK sit high
//		SPI.end();

//		gpio_init(PIN_CMD);
//		gpio_set_dir(PIN_CMD, GPIO_OUT);
//		gpio_put(PIN_CMD, HIGH);

//		gpio_init(PIN_CLK);
//		gpio_set_dir(PIN_CLK, GPIO_OUT);
//		gpio_put(PIN_CLK, HIGH);
		
		gpio_put(PIN_ATT, HIGH);
		
		delayMicroseconds (ATTN_DELAY);
	}
	
	virtual byte shiftInOut (const byte out) override {
		return SPI.transfer (out);
	}

public:
	virtual boolean begin () override {


//		gpio_init(PIN_ATT);					//SS(GPIO)
//		gpio_set_dir(PIN_ATT, GPIO_OUT);
		gpio_put(PIN_ATT, HIGH);
		
		/* We need to force these at startup, that's why we need to know which
		 * pins are used for HW SPI. It's a sort of "start condition" the
		 * controller needs.
		 */
//		gpio_init(PIN_CMD);					//MOSI
//		gpio_set_dir(PIN_CMD, GPIO_OUT);
//		gpio_put(PIN_CMD, HIGH);

//		gpio_init(PIN_CLK);					//SCK
//		gpio_set_dir(PIN_CLK, GPIO_OUT);
//		gpio_put(PIN_CLK, HIGH);

//		gpio_init(PIN_DAT);					//MISO
//		gpio_set_dir(PIN_DAT, GPIO_IN);
//    gpio_pull_up(PIN_DAT);


//		gpio_put(25, LOW);

		SPI.begin ();

		return PsxController::begin ();
	}
};